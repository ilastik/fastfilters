from waflib import Task, Options, Configure, TaskGen, Logs, Build, Utils, Errors
from waflib.TaskGen import feature, before_method
from waflib.Configure import conf
from waflib.Tools import waf_unit_test

VERSION='0.0.1'
APPNAME='libfastfilters'

srcdir = 'src'
blddir = 'build'

AVXTEST = '''#include <immintrin.h>
    #include <stdlib.h>
    #include <stdio.h>
    int main()
    {
    __m256 a = _mm256_set1_ps(rand());
    __m256 b = _mm256_set1_ps(rand());
    b = _mm256_fmadd_ps(a, a, b);
    float result = _mm_cvtss_f32(_mm256_extractf128_ps(b, 0));
    printf("%f\\n", result);
    return 0;
    }
'''

@conf
def check_avx2(self):
	flags = ["/arch:AVX2", "-mavx -mavx2 -mfma"]
	msg='Testing AVX2/FMA support'

	self.start_msg('Checking AVX2/FMA compiler support')
	for flag in flags:
		try:
			self.check(header_name='immintrin.h', define_name='HAVE_AVX2_FMA', msg=msg,
				fragment=AVXTEST, errmsg='Compiler does not support AVX2/FMA instruction set.',
				features='cxx', cxxflags=flag, uselib_store="AVX2_FMA")
		except self.errors.ConfigurationError:
			continue
		else:
			self.end_msg("yes (%s)" % flag)
			return flag
	self.fatal('Compiler does not support AVX2/FMA instruction set.')

@feature('avx2')
def feature_avx2(self):
	self.env.append_value('CXXFLAGS', self.env.CXXFLAGS_AVX2_FMA)

@conf
def check_cpu_avx2(self):
	self.start_msg('Checking AVX2/FMA CPU support')
	try:
		self.check(header_name='immintrin.h', msg='',
			fragment=AVXTEST, errmsg='CPU does not support AVX2/FMA instruction set.',
			features='cxx cxxprogram', cxxflags=self.env.CXXFLAGS_AVX2_FMA, uselib_store="AVX2_FMA_CPU",
			execute=True)
	except self.errors.ConfigurationError:
		self.end_msg("no. AVX tests will be skipped.", color='RED')
	else:
		self.end_msg("yes.")

@conf
def check_cxx11(self):
	self.check(msg='Checking for C++11 support', errmsg='Compiler does not support C++11.', features='cxx', cxxflags='-std=c++11')

@conf
def check_vigra(self, includes=[]):
	self.check(header_name='vigra/config_version.hxx', features='cxx', msg='Checking for vigra headers', includes=includes, uselib_store='HAVE_VIGRA')

	self.check_cxx(
		header_name='vigra/config_version.hxx',
		fragment='''
			#include <vigra/config_version.hxx>
			#include <iostream>
			#include <exception>
			#include <stdexcept>
			int main()
			{
				#if VIGRA_VERSION_MAJOR == 1 && VIGRA_VERSION_MINOR >= 11
				std::cout << VIGRA_VERSION;
				#else
				throw std::runtime_error("Unsupported vigra version.");
				#endif
			}
		''',
		execute=True,
		uselib_store='VIGRA',
		msg='Checking for VIGRA',
		define_name='VIGRA_VERSION2',
		includes=includes,
		use='vigra',
		var='have_vigra'
		)

@feature('vigra')
def feature_vigra(self):
	self.env.append_value('INCPATHS', self.env.INCLUDES_VIGRA)

@feature('opencv')
def feature_opencv(self):
	self.env.append_value('INCPATHS', self.env.INCLUDES_OPENCV)
	self.env.append_value('LIB', self.env.LIB_OPENCV)

def options(opt):
	opt.load('compiler_cxx')
	opt.load('python')
	opt.load('waf_unit_test')

	opt.add_option('--disable-python', help=("Don't build python bindings."), action='store_true', default=False, dest='python_disable')
	opt.add_option('--disable-tests', help=("Don't run tests."), action='store_true', default=False, dest='tests_disable')

	opt.add_option('--vigra-includes', help=("VIGRA include directory for performance tests."), action='store', default='', dest='vigra_incdir')
	opt.add_option('--without-vigra', help=("Disable VIGRA tests."), action='store_true', default=False, dest='vigra_disable')

	opt.add_option('--without-opencv', help=("Disable OpenCV tests."), action='store_true', default=False, dest='opencv_disable')

	opt.add_option('--debug', help=("Compile with debug symbols."), action='store_true', default=False, dest='enable_debug')

def configure(cfg):
	cfg.load('compiler_cxx')
	cfg.load('waf_unit_test')

	cfg.check_cxx11()
	cfg.check_avx2()
	cfg.check_cpu_avx2()

	if not cfg.options.vigra_disable:
		cfg.check_vigra([cfg.options.vigra_incdir])

	if not cfg.options.opencv_disable:
		cfg.check_cfg(package='opencv', args='--cflags --libs', uselib_store='OPENCV', use='opencv')

	if not cfg.options.python_disable:
		cfg.load('python')
		cfg.check_python_version((2,3))
		cfg.check_python_headers()

	cfg.env.append_value('INCLUDES', ['pybind11/include', 'include'])
	cfg.env.append_value('CXXFLAGS', ['-std=c++11', '-W', '-Wall', '-O3'])

	if cfg.options.enable_debug:
		cfg.env.append_value("CXXFLAGS", ['-g'])

	cfg.env.python_disable = cfg.options.python_disable
	cfg.env.tests_disable = cfg.options.tests_disable
	cfg.env.vigra_disable = cfg.options.vigra_disable
	cfg.env.opencv_disable = cfg.options.opencv_disable

def build(bld):
	sources_noavx = ["src/avx.cxx", "src/convolve_fir.cxx", "src/convolve_fir_nosimd.cxx", "src/convolve_iir.cxx", "src/convolve_iir_deriche.cxx"]
	sources_avx = ["src/convolve_iir_avx.cxx", "src/convolve_fir_avx.cxx"]
	sources_python = ["src/pybind.cxx"]

	TaskGen.declare_chain(
						name      = 'copy_bin',
						rule      = "cp ${SRC} ${TGT}",
						ext_in    = '.bin',
						ext_out   = '.b',
						before    = 'utest',
						reentrant = False)

	bld.objects(
		source  = sources_noavx,
		target  = 'objs_noavx',
		uselib  = 'cxxshlib')
	bld.objects(
		source  = sources_avx,
		target  = 'objs_avx',
		features='avx2',
		uselib  = 'cxxshlib')

	bld.shlib(features='cxx', source=["src/dummy.cxx"], target='fastfilters', use="objs_avx objs_noavx", name="fastfilters_shared")
	bld(features='cxx cxxstlib', source=["src/dummy.cxx"], target='fastfilters', use="objs_avx objs_noavx", name="fastfilters_static")

	if not bld.env.python_disable:
		bld.shlib(features='pyext', source=sources_python, target='fastfilters', use="fastfilters_shared", name="fastfilters_pyext")

	if not bld.env.tests_disable:
		tests_common = bld.path.ant_glob("tests/*.bin")

		tests = ["iir.cxx"]
		tests_avx = []
		tests_vigra = ["vigra.cxx"]
		tests_opencv = ["opencv.cxx"]

		for test in tests:
			bld.program(features='cxx test', source=["tests/" + test] + tests_common, target="test_" + test[:-4], use="fastfilters_shared")


		if 'CXXFLAGS_AVX2_FMA_CPU' in bld.env.keys():
			for test in tests_avx:
				bld.program(features='cxx test', source=["tests/" + test], target="test_" + test[:-4], use="fastfilters_shared")

		if not bld.env.vigra_disable:
			for test in tests_vigra:
				bld.program(features='cxx test vigra', source=["tests/" + test], target="test_" + test[:-4], use="fastfilters_shared")

		if not bld.env.opencv_disable:
			for test in tests_opencv:
				bld.program(features='cxx test opencv', source=["tests/" + test], target="test_" + test[:-4], use="fastfilters_shared")

		bld.options.all_tests = True
		bld.add_post_fun(waf_unit_test.summary)
		bld.add_post_fun(waf_unit_test.set_exit_code)
