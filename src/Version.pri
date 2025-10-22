# Version Information
VERSION = 0.1.0 
APP_VERSION = $$VERSION

# 定义传递给编译器的版本宏
DEFINES += APP_VERSION_STR=\\\"$$VERSION\\\"

defineTest(versionAtLeast) {
    !greaterThan($$1, $$2):lessThan($$1, $$2) {
        return(false)
    }
    return(true)
}