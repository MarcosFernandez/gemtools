/*
 * PROJECT: GEM-Tools library
 * FILE: gt.mapset.c
 * DATE: 08/11/2012
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: Utility to perform set operations {UNION,INTERSECTION,DIFFERENCE} over alignment files {MAP,SAM}
 */

#include <getopt.h>
#include <omp.h>

#include "gem_tools.h"

typedef enum { GT_MAP_SET_INTERSECTION, GT_MAP_SET_UNION, GT_MAP_SET_DIFFERENCE,
               GT_MAP_SET_JOIN, GT_MAP_SET_COMPARE,
               GT_MERGE_MAP, GT_DISPLAY_COMPACT_MAP} gt_operation;

typedef struct {
  char* name_input_file_1;
  char* name_input_file_2;
  char* name_output_file;
  gt_operation operation;
  bool mmap_input;
  bool paired_end;
  bool files_contain_same_reads;
  double eq_threshold;
  bool strict;
  bool verbose;
  uint64_t num_threads;
} gt_stats_args;

gt_stats_args parameters = {
    .name_input_file_1=NULL,
    .name_input_file_2=NULL,
    .name_output_file=NULL,
    .mmap_input=false,
    .paired_end=false,
    .files_contain_same_reads=true,
    .eq_threshold=0.5,
    .strict=false,
    .verbose=false,
    .num_threads=1
};
uint64_t current_read_length;

int64_t gt_mapset_map_cmp(gt_map* const map_1,gt_map* const map_2) {
  const uint64_t eq_threshold = (parameters.eq_threshold <= 1.0) ?
      parameters.eq_threshold*current_read_length: parameters.eq_threshold;
  return parameters.strict ? gt_map_cmp(map_1,map_2) : gt_map_range_cmp(map_1,map_2,eq_threshold);
}
int64_t gt_mapset_mmap_cmp(gt_map** const map_1,gt_map** const map_2,const uint64_t num_maps) {
  const uint64_t eq_threshold = (parameters.eq_threshold <= 1.0) ?
      parameters.eq_threshold*current_read_length: parameters.eq_threshold;
  return parameters.strict ? gt_mmap_cmp(map_1,map_2,num_maps) : gt_mmap_range_cmp(map_1,map_2,num_maps,eq_threshold);
}

GT_INLINE gt_status gt_mapset_read_template_sync(
    gt_buffered_input_file* const buffered_input_master,gt_buffered_input_file* const buffered_input_slave,
    gt_buffered_output_file* const buffered_output,gt_template* const template_master,gt_template* const template_slave,
    const gt_operation operation) {
  // Read master
  gt_status error_code_master, error_code_slave;
  gt_output_map_attributes* output_attributes = gt_output_map_attributes_new();
  gt_generic_parser_attributes* generic_parser_attr = gt_input_generic_parser_attributes_new(parameters.paired_end);
  if ((error_code_master=gt_input_generic_parser_get_template(
      buffered_input_master,template_master,generic_parser_attr))==GT_IMP_FAIL) {
    gt_fatal_error_msg("Fatal error parsing file <<Master>>");
  }
  // Read slave
  if ((error_code_slave=gt_input_generic_parser_get_template(
      buffered_input_slave,template_slave,generic_parser_attr))==GT_IMP_FAIL) {
    gt_fatal_error_msg("Fatal error parsing file <<Slave>>");
  }
  // Check EOF conditions
  if (error_code_master==GT_IMP_EOF) {
    if (error_code_slave!=GT_IMP_EOF) {
      gt_fatal_error_msg("<<Slave>> contains more/different reads from <<Master>>");
    }
    return GT_IMP_EOF;
  } else if (error_code_slave==GT_IMP_EOF) { // Slave exhausted. Dump master & return EOF
    do {
      if (error_code_master==GT_IMP_FAIL) gt_fatal_error_msg("Fatal error parsing file <<Master>>");
      if (operation==GT_MAP_SET_UNION || operation==GT_MAP_SET_DIFFERENCE) {
        gt_output_map_bofprint_template(buffered_output,template_master,output_attributes);
      }
    } while ((error_code_master=gt_input_generic_parser_get_template(
                buffered_input_master,template_master,generic_parser_attr)));
    return GT_IMP_EOF;
  }
  // Synch loop
  while (gt_string_cmp(gt_template_get_string_tag(template_master),
      gt_template_get_string_tag(template_slave))) {
    // Print non correlative master's template
    if (operation==GT_MAP_SET_UNION || operation==GT_MAP_SET_DIFFERENCE) {
      gt_output_map_bofprint_template(buffered_output,template_master,output_attributes);
    }
    // Fetch next master's template
    if ((error_code_master=gt_input_generic_parser_get_template(
        buffered_input_master,template_master,generic_parser_attr))!=GT_IMP_OK) {
      gt_fatal_error_msg("<<Slave>> contains more/different reads from <<Master>>");
    }
  }
  return GT_IMP_OK;
}

GT_INLINE gt_status gt_mapset_read_template_get_commom_map(
    gt_buffered_input_file* const buffered_input_master,gt_buffered_input_file* const buffered_input_slave,
    gt_template* const template_master,gt_template* const template_slave) {
  gt_status error_code_master, error_code_slave;
  gt_generic_parser_attributes* generic_parser_attr = gt_input_generic_parser_attributes_new(parameters.paired_end);
  // Read master
  if ((error_code_master=gt_input_generic_parser_get_template(
      buffered_input_master,template_master,generic_parser_attr))==GT_IMP_FAIL) {
    gt_fatal_error_msg("Fatal error parsing file <<Master>>");
  }
  if (error_code_master==GT_IMP_EOF) return GT_IMP_EOF;
  // Read slave
  if ((error_code_slave=gt_input_generic_parser_get_template(
      buffered_input_slave,template_slave,generic_parser_attr))==GT_IMP_FAIL) {
    gt_fatal_error_msg("Fatal error parsing file <<Slave>>");
  }
  if (error_code_slave==GT_IMP_EOF) { // Check EOF conditions
    gt_fatal_error_msg("<<Slave>> is not contained in master <<Master>> (looking for '"PRIgts"')",
        PRIgts_content(gt_template_get_string_tag(template_master)));
  }
  // Synch loop
  while (gt_string_cmp(gt_template_get_string_tag(template_master),gt_template_get_string_tag(template_slave))) {
    // Fetch next slave's template
    if ((error_code_master=gt_input_generic_parser_get_template(
        buffered_input_slave,template_slave,generic_parser_attr))!=GT_IMP_OK) {
      gt_fatal_error_msg("<<Slave>> is not contained in master <<Master>> (looking for '"PRIgts"')",
          PRIgts_content(gt_template_get_string_tag(template_master)));
    }
  }
  return GT_IMP_OK;
}

void gt_mapset_perform_set_operations() {
  // File IN/OUT
  gt_input_file* input_file_1 = gt_input_file_open(parameters.name_input_file_1,parameters.mmap_input);
  gt_input_file* input_file_2 = (parameters.name_input_file_2==NULL) ?
      gt_input_stream_open(stdin) : gt_input_file_open(parameters.name_input_file_2,parameters.mmap_input);
  if (parameters.name_input_file_2==NULL) GT_SWAP(input_file_1,input_file_2);
  gt_output_file* output_file = (parameters.name_output_file==NULL) ?
      gt_output_stream_new(stdout,SORTED_FILE) : gt_output_file_new(parameters.name_output_file,SORTED_FILE);

  // Buffered I/O
  gt_buffered_input_file* buffered_input_1 = gt_buffered_input_file_new(input_file_1);
  gt_buffered_input_file* buffered_input_2 = gt_buffered_input_file_new(input_file_2);
  gt_buffered_output_file* buffered_output = gt_buffered_output_file_new(output_file);
  gt_buffered_input_file_attach_buffered_output(buffered_input_1,buffered_output);

  // Template I/O (synch)
  gt_template *template_1 = gt_template_new();
  gt_template *template_2 = gt_template_new();
  gt_output_map_attributes* output_attributes = gt_output_map_attributes_new();
  while (gt_mapset_read_template_sync(buffered_input_1,buffered_input_2,
      buffered_output,template_1,template_2,parameters.operation)) {
    // Record current read length
    current_read_length = gt_template_get_total_length(template_1);
    // Apply operation
    gt_template *ptemplate;
    switch (parameters.operation) {
      case GT_MAP_SET_UNION:
        ptemplate=gt_template_union_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_1,template_2);
        break;
      case GT_MAP_SET_INTERSECTION:
        ptemplate=gt_template_intersect_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_1,template_2);
        break;
      case GT_MAP_SET_DIFFERENCE:
        ptemplate=gt_template_subtract_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_1,template_2);
        break;
      default:
        gt_fatal_error(SELECTION_NOT_VALID);
        break;
    }
    // Print template
    gt_output_map_bofprint_template(buffered_output,ptemplate,output_attributes);
    // Delete template
    gt_template_delete(ptemplate);
  }

  // Clean
  gt_template_delete(template_1);
  gt_template_delete(template_2);
  gt_buffered_input_file_close(buffered_input_1);
  gt_buffered_input_file_close(buffered_input_2);
  gt_buffered_output_file_close(buffered_output);
  gt_input_file_close(input_file_1);
  gt_input_file_close(input_file_2);
  gt_output_file_close(output_file);
}

void gt_mapset_perform_cmp_operations() {
  // File IN/OUT
  gt_input_file* input_file_1 = gt_input_file_open(parameters.name_input_file_1,parameters.mmap_input);
  gt_input_file* input_file_2 = (parameters.name_input_file_2==NULL) ?
      gt_input_stream_open(stdin) : gt_input_file_open(parameters.name_input_file_2,parameters.mmap_input);
  if (parameters.name_input_file_2==NULL) GT_SWAP(input_file_1,input_file_2);
  gt_output_file* output_file = (parameters.name_output_file==NULL) ?
      gt_output_stream_new(stdout,SORTED_FILE) : gt_output_file_new(parameters.name_output_file,SORTED_FILE);

  // Buffered I/O
  gt_buffered_input_file* buffered_input_1 = gt_buffered_input_file_new(input_file_1);
  gt_buffered_input_file* buffered_input_2 = gt_buffered_input_file_new(input_file_2);
  gt_buffered_output_file* buffered_output = gt_buffered_output_file_new(output_file);
  gt_buffered_input_file_attach_buffered_output(buffered_input_1,buffered_output);

  // Template I/O (synch)
  gt_template *template_1 = gt_template_new();
  gt_template *template_2 = gt_template_new();
  gt_output_map_attributes* output_map_attributes = gt_output_map_attributes_new();
  while (gt_mapset_read_template_get_commom_map(buffered_input_1,buffered_input_2,template_1,template_2)) {
    // Record current read length
    current_read_length = gt_template_get_total_length(template_1);
    // Apply operation
    switch (parameters.operation) {
      case GT_MAP_SET_JOIN:
        // Print Master's TAG+Counters+Maps
        gt_output_map_bofprint_tag(buffered_output,template_1->tag,template_1->attributes,output_map_attributes);
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_counters(buffered_output,gt_template_get_counters_vector(template_1),
            template_1->attributes,output_map_attributes); // Master's Counters
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_counters(buffered_output,gt_template_get_counters_vector(template_2),
            template_1->attributes,output_map_attributes); // Slave's Counters
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_template_maps(buffered_output,template_1,output_map_attributes); // Master's Maps
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_template_maps(buffered_output,template_2,output_map_attributes); // Slave's Maps
        gt_bofprintf(buffered_output,"\n");
        break;
      case GT_MAP_SET_COMPARE: {
        // Perform simple cmp operations
        gt_template *template_master_minus_slave=gt_template_subtract_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_1,template_2);
        gt_template *template_slave_minus_master=gt_template_subtract_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_2,template_1);
        gt_template *template_intersection=gt_template_intersect_template_mmaps_fx(gt_mapset_mmap_cmp,gt_mapset_map_cmp,template_1,template_2);
        /*
         * Print results :: (TAG (Master-Slave){COUNTER MAPS} (Slave-Master){COUNTER MAPS} (Intersection){COUNTER MAPS})
         */
        gt_output_map_bofprint_tag(buffered_output,template_1->tag,template_1->attributes,output_map_attributes);
        // Counters
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_counters(buffered_output,gt_template_get_counters_vector(template_master_minus_slave),
            template_master_minus_slave->attributes,output_map_attributes); // (Master-Slave){COUNTER}
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_counters(buffered_output,gt_template_get_counters_vector(template_slave_minus_master),
            template_slave_minus_master->attributes,output_map_attributes); // (Slave-Master){COUNTER}
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_counters(buffered_output,gt_template_get_counters_vector(template_intersection),
            template_intersection->attributes,output_map_attributes); // (Intersection){COUNTER}
        // Maps
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_template_maps(buffered_output,template_master_minus_slave,output_map_attributes); // (Master-Slave){COUNTER}
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_template_maps(buffered_output,template_slave_minus_master,output_map_attributes); // (Slave-Master){COUNTER}
        gt_bofprintf(buffered_output,"\t");
        gt_output_map_bofprint_template_maps(buffered_output,template_intersection,output_map_attributes); // (Intersection){COUNTER}
        gt_bofprintf(buffered_output,"\n");
        // Delete templates
        gt_template_delete(template_master_minus_slave);
        gt_template_delete(template_slave_minus_master);
        gt_template_delete(template_intersection);
        }
        break;
      default:
        gt_fatal_error(SELECTION_NOT_VALID);
        break;
    }
  }

  // Clean
  gt_template_delete(template_1);
  gt_template_delete(template_2);
  gt_buffered_input_file_close(buffered_input_1);
  gt_buffered_input_file_close(buffered_input_2);
  gt_buffered_output_file_close(buffered_output);
  gt_input_file_close(input_file_1);
  gt_input_file_close(input_file_2);
  gt_output_file_close(output_file);
}

void gt_mapset_perform_merge_map() {
  // Open file IN/OUT
  gt_input_file* input_file_1 = gt_input_file_open(parameters.name_input_file_1,parameters.mmap_input);
  gt_input_file* input_file_2 = (parameters.name_input_file_2==NULL) ?
      gt_input_stream_open(stdin) : gt_input_file_open(parameters.name_input_file_2,parameters.mmap_input);
  if (parameters.name_input_file_2==NULL) GT_SWAP(input_file_1,input_file_2);
  gt_output_file* output_file = (parameters.name_output_file==NULL) ?
      gt_output_stream_new(stdout,SORTED_FILE) : gt_output_file_new(parameters.name_output_file,SORTED_FILE);

  // Mutex
  pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

  // Parallel reading+process
  #pragma omp parallel num_threads(parameters.num_threads)
  {
    if (parameters.files_contain_same_reads) {
      gt_merge_synch_map_files(&input_mutex,parameters.paired_end,output_file,input_file_1,input_file_2);
    } else {
      gt_merge_unsynch_map_files(&input_mutex,input_file_1,input_file_2,parameters.paired_end,output_file);
    }
  }

  // Clean
  gt_input_file_close(input_file_1);
  gt_input_file_close(input_file_2);
  gt_output_file_close(output_file);
}
void gt_mapset_display_compact_map() {
  // Open file IN/OUT
  gt_input_file* input_file = (parameters.name_input_file_1==NULL) ?
      gt_input_stream_open(stdin) : gt_input_file_open(parameters.name_input_file_1,parameters.mmap_input);
  gt_output_file* output_file = (parameters.name_output_file==NULL) ?
      gt_output_stream_new(stdout,SORTED_FILE) : gt_output_file_new(parameters.name_output_file,SORTED_FILE);

  #pragma omp parallel num_threads(parameters.num_threads)
  {
    gt_output_map_attributes* const output_map_attributes = gt_output_map_attributes_new();
    output_map_attributes->compact = true;
    GT_BEGIN_READING_WRITING_LOOP(input_file,output_file,parameters.paired_end,buffered_output,template) {
      GT_TEMPLATE_ITERATE_ALIGNMENT(template,alignment) {
        // Print compact summary
        gt_bofprintf(buffered_output,"End1::"PRIgts"[%"PRIu64"]\t",PRIgts_content(alignment->tag),gt_string_get_length(alignment->read));
        gt_output_map_bofprint_counters(buffered_output,alignment->counters,alignment->attributes,output_map_attributes);
        gt_bofprintf(buffered_output,"\t");
        uint64_t printed = 0;
        GT_ALIGNMENT_ITERATE(alignment,map) {
          if (printed>0) {
            gt_bofprintf(buffered_output,","PRIgts,PRIgts_content(map->seq_name));
          } else {
            gt_bofprintf(buffered_output,PRIgts,PRIgts_content(map->seq_name));
          }
          ++printed;
        }
        gt_bofprintf(buffered_output,"\n");
      }
    } GT_END_READING_WRITING_LOOP(input_file,output_file,template);
    // Clean
    gt_output_map_attributes_delete(output_map_attributes);
  }

  // Clean
  gt_input_file_close(input_file);
  gt_output_file_close(output_file);
}
void usage() {
  fprintf(stderr, "USE: ./gt.mapset [OPERATION] [ARGS]...\n"
                  "       {OPERATION}\n"
                  "         [Set Operators]\n"
                  "           union\n"
                  "           intersection\n"
                  "           difference\n"
                  "         [Compare/Display Files]\n"
                  "           compare\n"
                  "           join\n"
                  "           display-compact\n"
                  "         [Map Specific]\n"
                  "           merge-map\n"
                  "       {ARGS}\n"
                  "         [I/O]\n"
                  "           --i1 [FILE]\n"
                  "           --i2 [FILE]\n"
                  "           --mmap-input\n"
                  "           --output|-o [FILE]\n"
                  "           --paired-end|p\n"
                  "           --files-same-reads|-s\n"
                  "         [Compare Function]\n"
                  "           --eq-th <number>|<float>\n"
                  "           --strict\n"
                  "         [Misc]\n"
                  "           --threads|t\n"
                  "           --verbose|v\n"
                  "           --help|h\n");
}

void parse_arguments(int argc,char** argv) {
#define GT_MAPSET_OPERATIONS "union,intersection,difference,compare,join,merge-map,display-compact"
  // Parse operation
  if (argc<=1) gt_fatal_error_msg("Please specify operation {"GT_MAPSET_OPERATIONS"}");
  if (gt_streq(argv[1],"INTERSECCTION") || gt_streq(argv[1],"Intersection") || gt_streq(argv[1],"intersection")) {
    parameters.operation = GT_MAP_SET_INTERSECTION;
  } else if (gt_streq(argv[1],"UNION") || gt_streq(argv[1],"Union") || gt_streq(argv[1],"union")) {
    parameters.operation = GT_MAP_SET_UNION;
  } else if (gt_streq(argv[1],"DIFFERENCE") || gt_streq(argv[1],"Difference") || gt_streq(argv[1],"difference")) {
    parameters.operation = GT_MAP_SET_DIFFERENCE;
  } else if (gt_streq(argv[1],"COMPARE") || gt_streq(argv[1],"Compare") || gt_streq(argv[1],"compare")) {
    parameters.operation = GT_MAP_SET_COMPARE;
  } else if (gt_streq(argv[1],"JOIN") || gt_streq(argv[1],"Join") || gt_streq(argv[1],"join")) {
    parameters.operation = GT_MAP_SET_JOIN;
  } else if (gt_streq(argv[1],"MERGE-MAP") || gt_streq(argv[1],"Merge-map") || gt_streq(argv[1],"merge-map")) {
    parameters.operation = GT_MERGE_MAP;
  } else if (gt_streq(argv[1],"DISPLAY-COMPACT") || gt_streq(argv[1],"Display-compact") || gt_streq(argv[1],"display-compact")) {
    parameters.operation = GT_DISPLAY_COMPACT_MAP;
  } else {
    if (argv[1][0]=='I' || argv[1][0]=='i') {
      fprintf(stderr,"\tAssuming 'Intersection' ...\n");
      parameters.operation = GT_MAP_SET_INTERSECTION;
    } else if (argv[1][0]=='U' || argv[1][0]=='u') {
      fprintf(stderr,"\tAssuming 'Union' ...\n");
      parameters.operation = GT_MAP_SET_UNION;
    } else if (argv[1][0]=='D' || argv[1][0]=='d') {
      fprintf(stderr,"\tAssuming 'Difference' ...\n");
      parameters.operation = GT_MAP_SET_DIFFERENCE;
    } else if (argv[1][0]=='C' || argv[1][0]=='c') {
      fprintf(stderr,"\tAssuming 'Compare' ...\n");
      parameters.operation = GT_MAP_SET_COMPARE;
    } else if (argv[1][0]=='P' || argv[1][0]=='p') {
      fprintf(stderr,"\tAssuming 'Join' ...\n");
      parameters.operation = GT_MAP_SET_JOIN;
    } else if (argv[1][0]=='M' || argv[1][0]=='m') {
      fprintf(stderr,"\tAssuming 'Merge-map' ...\n");
      parameters.operation = GT_MERGE_MAP;
    } else {
      gt_fatal_error_msg("Unknown operation '%s' in {"GT_MAPSET_OPERATIONS"}",argv[1]);
    }
  }
  argc--; argv++;
  // Parse arguments
  struct option long_options[] = {
    { "i1", required_argument, 0, 1 },
    { "i2", required_argument, 0, 2 },
    { "mmap-input", no_argument, 0, 3 },
    { "output", required_argument, 0, 'o' },
    { "paired-end", no_argument, 0, 'p' },
    { "files-same-reads", no_argument, 0, 's' },
    /* CMP */
    { "eq-th", required_argument, 0, 4 },
    { "strict", no_argument, 0, 5 },
    /* MISC */
    { "threads", required_argument, 0, 't' },
    { "verbose", no_argument, 0, 'v' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 } };
  int c,option_index;
  while (1) {
    c=getopt_long(argc,argv,"i:o:psht:v",long_options,&option_index);
    if (c==-1) break;
    switch (c) {
    case 1:
      parameters.name_input_file_1 = optarg;
      break;
    case 2:
      parameters.name_input_file_2 = optarg;
      break;
    case 'o':
      parameters.name_output_file = optarg;
      break;
    case 3:
      parameters.mmap_input = true;
      break;
    case 'p':
      parameters.paired_end = true;
      break;
    case 's':
      parameters.files_contain_same_reads = true;
      break;
    case 4:
      parameters.eq_threshold = atof(optarg);
      break;
    case 5:
      parameters.strict = true;
      break;
    case 't':
      parameters.num_threads = atol(optarg);
      break;
    case 'v':
      parameters.verbose = true;
      break;
    case 'h':
      usage();
      exit(1);
    case '?':
    default:
      gt_fatal_error_msg("Option not recognized");
    }
  }
  // Check parameters
  if (parameters.operation!=GT_DISPLAY_COMPACT_MAP && !parameters.name_input_file_1) {
    gt_fatal_error_msg("Input file 1 required (--i1)\n");
  }
}

int main(int argc,char** argv) {
  // GT error handler
  gt_handle_error_signals();

  // Parsing command-line options
  parse_arguments(argc,argv);

  // Filter !
  if (parameters.operation==GT_MERGE_MAP) {
    gt_mapset_perform_merge_map();
  } else if (parameters.operation==GT_DISPLAY_COMPACT_MAP) {
    gt_mapset_display_compact_map();
  } else if (parameters.operation==GT_MAP_SET_INTERSECTION ||
      parameters.operation==GT_MAP_SET_UNION ||
      parameters.operation==GT_MAP_SET_DIFFERENCE) {
    gt_mapset_perform_set_operations();
  } else {
    gt_mapset_perform_cmp_operations();
  }

  return 0;
}


