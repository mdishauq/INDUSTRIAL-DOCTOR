Import("env")

env.Replace(
    ASPPCOM = "$CC -x assembler-with-cpp -c $ASPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -o $TARGET $SOURCES"
)