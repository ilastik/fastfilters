Originally developed by [Sven Peter](https://github.com/svenpeter42), now maintained by the [ilastik team](https://github.com/ilastik)

[![conda-version](https://anaconda.org/ilastik-forge/fastfilters/badges/version.svg)](https://anaconda.org/ilastik-forge/fastfilters)
[![conda-updated](https://anaconda.org/ilastik-forge/fastfilters/badges/latest_release_date.svg)](https://anaconda.org/ilastik-forge/fastfilters)
[![conda-platforms](https://anaconda.org/ilastik-forge/fastfilters/badges/platforms.svg)](https://anaconda.org/ilastik-forge/fastfilters)
[![conda-dl](https://anaconda.org/ilastik-forge/fastfilters/badges/downloads.svg)](https://anaconda.org/ilastik-forge/fastfilters)

Build status:

main: [![test](https://github.com/ilastik/fastfilters/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/ilastik/fastfilters/actions/workflows/test.yml)

devel: [![test](https://github.com/ilastik/fastfilters/actions/workflows/test.yml/badge.svg?branch=devel)](https://github.com/ilastik/fastfilters/actions/workflows/test.yml)

Installation (stable)
------------

	% git clone https://github.com/svenpeter42/fastfilters.git
	% cd fastfilters
	% mkdir build
	% cmake ..
	% make
	% make install


Conda Installation (stable)
------------

	% conda install -c ilastik-forge -c conda-forge fastfilters


Gentoo Installation (development) (**not maintained**)
------------

	% git clone https://github.com/svenpeter42/fastfilters.git
	% cd fastfilters/pkg/gentoo/sci-libs/fastfilters
	% sudo ebuild fastfilters-9999.ebuild manifest clean merge
