#include "Python.h"
#include "gem_tools.h"
#include "py_mismatch.h"
#include "py_map.h"
#include "py_alignment.h"
#include "py_template.h"
#include "py_template_iterator.h"
#include "py_iterator.h"

/******* TEMPLATE ********/
static PyGetSetDef Template_getseters[] = {
    {"tag", (getter) Template_gettag, (setter) Template_settag, "Template Tag", NULL},
    {"max_complete_strata", (getter) Template_getmax_complete_strata, 
        (setter) Template_setmax_complete_strata, "Max Complete Strata", NULL},
    {"blocks", (getter) Template_getblocks, 
        (setter) Template_setblocks, "Alignment blocks", NULL},
    {"counters", (getter) Template_getcounters, 
        (setter) Template_setcounters, "Counters", NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject TemplateType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gempy.Template",             /*tp_name*/
    sizeof(Template),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Template objects",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,             /* tp_methods */
    0,             /* tp_members */
    Template_getseters,        /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Template_init,      /* tp_init */
    0,                         /* tp_alloc */
    Template_new,                 /* tp_new */
};

Template* create_template(gt_template* template){
    Template* tmpl = PyObject_New(Template, &TemplateType);
    tmpl->template = template;
    return tmpl;
}
/******* END TEMPLATE ********/


/******* TEMPLATE ITERATOR ********/
static PyTypeObject gempy_template_iteratorType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gem.gemtools._template_iterator",            /*tp_name*/
    sizeof(gempy_template_iterator),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    gempy_template_iterator_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
      /* tp_flags: Py_TPFLAGS_HAVE_ITER tells python to
         use tp_iter and tp_iternext fields. */
    "Internal Template iterator",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    gempy_template_iterator_iter,  /* tp_iter: __iter__() method */
    gempy_template_iterator_iternext  /* tp_iternext: next() method */
};

gempy_template_iterator* create_template_stream_iterator(FILE* file){
    gempy_template_iterator *p;
    p = PyObject_New(gempy_template_iterator, &gempy_template_iteratorType);
    if (!p) return NULL;
    p->input_file = gt_input_stream_open(file);
    p->map_input = gt_buffered_map_input_new(p->input_file);
    p->template = gt_template_new();
    return (gempy_template_iterator *)p;
}

gempy_template_iterator* create_template_file_iterator(char* filename, bool memorymap){
    gempy_template_iterator *p;
    p = PyObject_New(gempy_template_iterator, &gempy_template_iteratorType);
    if (!p) return NULL;
    /* I'm not sure if it's strictly necessary. */
    if (!PyObject_Init((PyObject *)p, &gempy_template_iteratorType)) {
        printf("ERROR template iterator init failed!\n");
        Py_DECREF(p);
        return NULL;
    }
    p->input_file = gt_input_file_open(filename, memorymap);
    p->map_input = gt_buffered_map_input_new(p->input_file); // false disable memory map
    p->template = gt_template_new();
    return (gempy_template_iterator *)p;
}
/******* END TEMPLATE ITERATOR ********/



/******* ALIGNMENT  ********/
static PyMethodDef Alignmnt_methods[] = {
    {"to_sequence", Alignment_to_sequence, METH_VARARGS, "Convert alignment Alignment_to_sequence"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyGetSetDef Alignment_getseters[] = {
    {"tag", (getter) Alignment_gettag, (setter) Alignment_settag, "Alignment Tag", NULL},
    {"read", (getter) Alignment_getread, (setter) Alignment_setread, "Alignment Read", NULL},
    {"qualities", (getter) Alignment_getqualities, (setter) Alignment_setqualities, "Alignment Qualities", NULL},
    {"max_complete_strata", (getter) Alignment_getmax_complete_strata, 
        (setter) Alignment_setmax_complete_strata, "Max Complete Strata", NULL},
    {"counters", (getter) Alignment_getcounters, 
        (setter) Alignment_setcounters, "Counters", NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject AlignmentType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gempy.Alignment",             /*tp_name*/
    sizeof(Alignment),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Alignment objects",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    Alignmnt_methods,             /* tp_methods */
    0,             /* tp_members */
    Alignment_getseters,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Alignment_init,      /* tp_init */
    0,                         /* tp_alloc */
    Alignment_new,                 /* tp_new */
};


Alignment* create_alignment(gt_alignment* alignment, gt_template* parent){
    Alignment* a = PyObject_New(Alignment, &AlignmentType);
    a->alignment = alignment;
    a->template = parent;
    return a;
}
/******* END ALIGNMENT  ********/


/******* MAP  ********/
static PyGetSetDef Map_getseters[] = {
    {"seq_name", (getter) Map_getseq_name, (setter) Map_setseq_name, "Genomic sequence name", NULL},
    {"position", (getter) Map_getposition, (setter) Map_setposition, "Genomic Position", NULL},
    {"base_length", (getter) Map_getbase_lengt, (setter) Map_setbase_length, "Base length", NULL},
    {"direction", (getter) Map_getdirection, (setter) Map_setdirection, "Strand", NULL},
    {"score", (getter) Map_getscore, (setter) Map_setscore, "Score", NULL},
    {"mismatches", (getter) Map_getmismatches, (setter) Map_setmismatches, "Mismatches", NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject MapType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gempy.Map",             /*tp_name*/
    sizeof(Map),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Map objects",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,             /* tp_methods */
    0,             /* tp_members */
    Map_getseters,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Map_init,      /* tp_init */
    0,                         /* tp_alloc */
    Map_new,                 /* tp_new */
};

Map* create_map(gt_map* map){
    Map* a = PyObject_New(Map, &MapType);
    a->map = map;
    return a;
}
/******* END MAP  ********/


/******* MISMATCH  ********/
static PyGetSetDef Mismatch_getseters[] = {
    {"type", (getter) Mismatch_gettype, (setter) Mismatch_settype, "Set type", NULL},
    {"position", (getter) Mismatch_getposition, (setter) Mismatch_setposition, "Set position", NULL},
    {"base", (getter) Mismatch_getbase, (setter) Mismatch_setbase, "Set base", NULL},
    {"size", (getter) Mismatch_getsize, (setter) Mismatch_setsize, "Set size", NULL},
    {NULL}  /* Sentinel */
};
static PyTypeObject MismatchType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gempy.Mismatch",          /*tp_name*/
    sizeof(Mismatch),          /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Mismatch objects",       /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                     /* tp_methods */
    0,                     /* tp_members */
    Mismatch_getseters,    /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Mismatch_init,      /* tp_init */
    0,                         /* tp_alloc */
    Mismatch_new,                 /* tp_new */
};

Mismatch* create_mismatch(gt_misms* map){
    Mismatch* a = PyObject_New(Mismatch, &MismatchType);
    a->misms = map;
    return a;
}
/******* END MISMATCH  ********/


/******* GENERIC ITERATOR ********/
static PyTypeObject gempy_iteratorType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "gempy._iterator",            /*tp_name*/
    sizeof(gempy_iterator),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
      /* tp_flags: Py_TPFLAGS_HAVE_ITER tells python to
         use tp_iter and tp_iternext fields. */
    "Internal iterator object.",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    gempy_iterator_iter,  /* tp_iter: __iter__() method */
    gempy_iterator_iternext  /* tp_iternext: next() method */
};


PyObject* create_gempy_iterator(uint64_t start, uint64_t length, void* getter, void* arg, void* converter, int parent){
    gempy_iterator *p;
    p = PyObject_New(gempy_iterator, &gempy_iteratorType);
    if (!p) return NULL;
    p->start = start;
    p->length = length;
    p->pos = 0;
    p->getter = getter;
    p->arg = arg;
    p->converter = converter;
    p->parent = parent;
    return (PyObject *)p;
}
/******* END GENERIC ITERATOR  ********/


/**** MODULE METHODS AND DEFINITIONS ******/

static PyObject* gempy_open_stream(PyObject *self, PyObject *args){
    void* m;
    gempy_template_iterator *p;
    if (!PyArg_ParseTuple(args, "O", &m))  return NULL;
    p = create_template_stream_iterator(PyFile_AsFile(m));
    if (!p) return NULL;
    return (PyObject *)p;
};

static PyObject* gempy_open_file(PyObject *self, PyObject *args){
    char* filename;
    PyObject* pp;
    gempy_template_iterator *p;
    if (!PyArg_ParseTuple(args, "s", &filename))  return NULL;
    p = create_template_file_iterator(filename, false);
    if (!p) return NULL;
    pp = (PyObject *)p;
    return pp;
};

static PyMethodDef GempyMethods[] = {
    {"open_file", gempy_open_file, METH_VARARGS, "Iterator over a .map file"},
    {"open_stream", gempy_open_stream, METH_VARARGS, "Iterator over a .map stream"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC initgemtools(void){
  PyObject* m;

  gempy_iteratorType.tp_new = PyType_GenericNew;
  gempy_template_iteratorType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&gempy_iteratorType) < 0)  return;
  if (PyType_Ready(&gempy_template_iteratorType) < 0)  return;
  if (PyType_Ready(&MismatchType) < 0)  return;
  if (PyType_Ready(&MapType) < 0)  return;
  if (PyType_Ready(&AlignmentType) < 0)  return;
  if (PyType_Ready(&TemplateType) < 0)  return;

  m = Py_InitModule("gemtools", GempyMethods);

  Py_INCREF(&gempy_iteratorType);
  Py_INCREF(&gempy_template_iteratorType);
  Py_INCREF(&TemplateType);
  Py_INCREF(&AlignmentType);
  Py_INCREF(&MapType);
  Py_INCREF(&MismatchType);

  PyModule_AddObject(m, "_template_iterator", (PyObject *)&gempy_template_iteratorType);
  PyModule_AddObject(m, "_iterator", (PyObject *)&gempy_iteratorType);
  PyModule_AddObject(m, "Template", (PyObject *)&TemplateType);
  PyModule_AddObject(m, "Alignment", (PyObject *)&AlignmentType);
  PyModule_AddObject(m, "Map", (PyObject *)&MapType);
  PyModule_AddObject(m, "Mismatch", (PyObject *)&MismatchType);
}
