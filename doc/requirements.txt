# Packages only needed to build the docs
Pygments>=2.2,<2.3
Sphinx>=1.6,<=1.7
sphinx-better-theme>=0.1.5,<0.2

# 0.15.2 affected by https://sourceforge.net/p/docutils/bugs/353/
# Can update to 0.16 after release (currently in rc) but must update Sphinx too
docutils<0.15
