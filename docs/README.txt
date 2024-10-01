CONTRIBUTING:

We welcome contributions to the documentation of ChampSim.

This documentation is built with Doxygen and Sphinx. Doxygen-style comments in C++ code are parsed into XML documents that can be parsed by Breathe.

All documentation sources are contained in the src directory.

To build documentation locally:

python3 -m pip install -r requirements.txt
sphinx-build -c . src _build/html
