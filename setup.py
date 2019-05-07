import setuptools

setuptools.setup(
    name='sonia',
    version='0.0.1',
    py_modules=['sonia'],
    setup_requires=["cffi>=1.0.0"],
    cffi_modules=["event_build.py:ffibuilder"],
    install_requires=["cffi>=1.0.0"],
)
