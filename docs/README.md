# LavaVu documentation generator

Most of these scripts borrow heavily from the `Underworld2 sphinx doc generation [scripts](https://github.com/underworldcode/underworld2/tree/master/docs/development/docs_generator)

To regenerate the API documents, run the following:

```bash
   sphinx-build . html 
```

The following python packages are required to build the documentation:

* sphinx
* sphinxcontrib-napoleon
* jupyter
* pandoc
* mock
* myst_parser

A requirements.txt file is provided, install them with

```bash
   pip install -r requirements.txt
```
