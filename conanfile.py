from conan import ConanFile
from conan.tools.cmake import CMakeToolchain

class App(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    requires = "boost/1.81.0"
    generators = "CMakeDeps"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.blocks.remove("cppstd") # While Clang not support C++23
        tc.generate()

