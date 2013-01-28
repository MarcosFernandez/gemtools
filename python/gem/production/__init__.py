#!/usr/bin/env python
import sys
import os
import time
import argparse
import logging
import gem
import gem.commands

from gem.filter import interleave, unmapped
from gem.filter import filter as gf
from gem.pipeline import MappingPipeline
from gem.utils import Command, CommandException

class Index(Command):
    title = "Index genomes"
    description = """This command can be used to index genomes
    """
    def register(self, parser):
        ## required parameters
        parser.add_argument('-i', '--input', dest="input", help='Path to a single uncompressed fasta file with the genome', required=True)
        parser.add_argument('-o', '--output', dest="output", help='Output file name (has to end in .gem), defaults to input file name + .gem extension')
        parser.add_argument('-t', '--threads', dest="threads", help='Number of threads', default=2)

    def run(self, args):
        input = args.input
        output = os.path.basename(input)
        if args.output is not None:
            output = args.output
        else:
            output = output[:output.rfind(".")] + ".gem"
        if not output.endswith(".gem"):
            raise CommandException("Output file name has to end in .gem")
        if not os.path.exists(input):
            raise CommandException("Input file not found : %s" % input)
        if input.endswith(".gz"):
            raise CommandException("Compressed input is currently not supported!")

        gem.index(input, output, threads=args.threads)


class TranscriptIndex(Command):
    title = "Create and index transcriptomes"
    description = """This command creates a transcriptome and its index from a gem
    index and a GTF annotation.

    Currently the output name is striclty taken from the name of the annotation
    file given. The command creates a set of files:

        <gtf>.junctions       -- the junction sites of the GTF
        <gtf>.junctions.fa    -- the transcriptome sequences
        <gtf>.junctions.keys  -- the translation table from transcriptome to genome coordinates
        <gtf>.gem             -- the GEM transcriptome index
    """
    def register(self, parser):
        ## required parameters
        parser.add_argument('-i', '--index', dest="index", help='Path to the GEM genome index', required=True)
        parser.add_argument('-a', '--annotation', dest="annotation", help='Path to the GTF annotation', required=True)
        parser.add_argument('-m', '--max-length', dest="maxlength", help='Maximum read length, defaults to 150', default=150)
        parser.add_argument('-t', '--threads', dest="threads", help='Number of threads', default=2)

    def run(self, args):
        if not args.index.endswith(".gem"):
            raise CommandException("No valid GEM index specified, the file has to end in .gem")
        if not os.path.exists(args.index):
            raise CommandException("GEM index not found")
        if not os.path.exists(args.annotation):
            raise CommandException("Annotation not found")

        junctions_out = os.path.basename(args.annotation) + ".junctions"
        index_out = os.path.basename(args.annotation) + ".gem"

        print "Loading Junctions"
        junctions = set(gem.junctions.from_gtf(args.annotation))
        print "%d Junctions loaded from annotation " % (len(junctions))
        gem.junctions.write_junctions(junctions, junctions_out)
        print "Junctions writen to %s " % (junctions_out)

        print "Computing transcriptome..."
        (transcriptome, keys) = gem.compute_transcriptome(args.maxlength, args.index, junctions_out)

        print "Indexing transcriptome"
        gem.index(transcriptome, index_out, threads=args.threads)
        print "Done"


class RnaPipeline(Command):
    description = "The RNASeq pipeline alignes reads"
    title = "Default RNASeq Pipeline"

    def register(self, parser):
        ## required parameters
        parser.add_argument('-f', '--files', dest="file", nargs="+", help='Single fastq input file or both pairs', required=True)
        parser.add_argument('-i', '--index', dest="index", help='Genome index', required=True)
        parser.add_argument('-m', '--max-read-length', dest="maxlength", help='The maximum read length', required=True)
        parser.add_argument('-a', '--annotation', dest="annotation", help='GTF annotation file', required=True)
        parser.add_argument('-r', '--transcript-index', dest="trans_index", help='Transcriptome index. If not specified it is assumed to be <gtf>.gem')
        parser.add_argument('-k', '--keys', dest="transcript_keys", help='Transcriptome .keys file. If not specified it is assumed to be <gtf>.junctions.keys')

        ## optional parameters
        parser.add_argument('-o', '--output-dir', dest="output", help='The output folder. If not specified the current working directory is used.')
        parser.add_argument('--junction-coverage', dest="junctioncoverage",
            help='A denovo junction must be covered by > coverage reads to be taken into account, 0 to disable', default=2)
        parser.add_argument('-s', '--strata-after-best', dest="delta",
            help='Number of strata that are examined after the best one', default=1)
        parser.add_argument('-g', '--no-gzip', dest="gzip", action="store_false", default=True, help="Do not compress result mapping")
        parser.add_argument('--keep-temp', dest="rmtemp", action="store_false", default=True, help="Keep temporary files")
        parser.add_argument('-t', '--threads', dest="threads", default=2, type=int, help="Number of threads to use")
        parser.add_argument('--no-sam', dest="nosam", action="store_true", default=False, help="Do not create sam/bam file")
        parser.add_argument('-n', '--name', dest="name", help="Name used for the results")
        parser.add_argument('--extend-name', dest="extendname", action="store_true", default=False, help="Extend the name by prefixing it with the parameter combination")

    def run(self, args):
        ## parsing command line arguments
        input_file = os.path.abspath(args.file[0])
        input_file2 = None
        name = None
        if len(args.file) > 1:
            input_file2 = os.path.abspath(args.file[1])
            name = os.path.basename(input_file)[:inputfile.rfind(".")]
        else:
            (name, input_file2) = gem.utils.find_pair(input_file)

        if args.name:
            name = args.name

        if args.extendname:
            name = "%s_%d_%.2f_%d" % (name, args.delta, args.unmappedthreshold, args.junctioncoverage)
        logging.info("Using dataset name: %s" % (name))

        pipeline = MappingPipeline(
            name=name,
            index=args.index,
            output_dir=args.output,
            annotation=args.annotation,
            threads=int(args.threads),
            junctioncoverage=int(args.junctioncoverage),
            maxlength=int(args.maxlength),
            transcript_index=args.trans_index,
            transcript_keys=args.transcript_keys,
            delta=int(args.delta),
            remove_temp=args.rmtemp
        )

        start_time = time.time()

        main_input = gem.files.open(input_file)
        main_input2 = gem.files.open(input_file2)

        # initial mapping
        initial_mapping = pipeline.mapping_step(interleave([main_input, main_input2], add_id=False), "initial")
        # create denovo transcriptome
        pipeline.create_denovo_transcriptome(initial_mapping)

        ## run initial transcript mapping
        initial_split_mapping = pipeline.transcript_mapping_step(initial_mapping.clone(), "initial")

        # merge
        #pipeline.merge("step_1")

        ## trim 20 mappings
        #pipeline.mapping_step(gf(initial_split_mapping, unmapped), "trim_20", trim=(0, 20))
        #pipeline.transcript_mapping_step(gf(initial_split_mapping, unmapped), "trim_20", trim=(0, 20))

        ## trium 5 mappings
        #pipeline.mapping_step(gf(initial_split_mapping, unmapped), "trim_5", trim=(5, 20))
        #pipeline.transcript_mapping_step(gf(initial_split_mapping, unmapped), "trim_5", trim=(5, 20))

        ## merge everything
        merged = pipeline.merge("merged")

        paired_mapping = pipeline.pair_align(merged, compress=args.gzip)

        if not args.nosam:
            pipeline.create_bam(paired_mapping, sort=True)

        pipeline.cleanup()

        end_time = (time.time() - start_time) / 60

        print "Completed job in %0.2f mins" % end_time


if __name__ == "__main__":
    main()

