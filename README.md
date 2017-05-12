### This code is associated with the paper "EM connectomics reveals axonal target variation in a sequence-generating network". eLife, 2017. http://dx.doi.org/10.7554/eLife.24364

Aligner
=======

### What it does well

**Stitches together any mode of serial section image data:**

- TEM
- Block-face SEM
- Fluorescence Array Tomography

**Fast and scalable:**

- Runs on linux cluster using Sun Grid Engine API
- Align 2 to billions of images
- Approx. linear time/volume & mem/volume scaling
- Two million 4MB images align in about 8 man-hours

**Handled pathologies:**

- Missing tiles or whole sections
- Fragmented / small / irregular sections
- Arbitrarily rotated/translated sections
- Burns, scars, foreign matter
- Exposure inhomogeneity

**Input:**

- 8 or 16 bit TIF, PNG, MRC
- Simple meta-data as text or TrakEM2 XML

**Output:**

- Basically 1 affine or homographic transform per image tile
- Flexible output as text tables or TrakEM2 XML files

### Limitations

- One linear transform / tile; **_not an elastic aligner_**
- Unfinished handling of geometry-altering folds and tears
- All images in a data set must share the same X and Y dimensions, though X need not equal Y

### Requirements

- Linux cluster with (Sun / Oracle / Univa) Grid Engine.
- [**Fiji**](http://fiji.sc/Fiji) + [**TrakEM2**](http://www.ini.uzh.ch/~acardona/trakem2.html): invaluable image and stack tools.

### Documentation

- Project folder **00_DOC** details installation and methods
- [Alignment_Tutorial](https://github.com/billkarsh/Alignment_Tutorial) walks you through a real-world example

### Authorship

Developed over several years at **HHMI/Janelia Research Campus**, originally by **Louis Scheffer**, and subsequently refined into current form by **Bill Karsh**. See reference ["Automated Alignment of Imperfect EM Images for Neural Reconstruction"](http://arxiv.org/abs/1304.6034).

