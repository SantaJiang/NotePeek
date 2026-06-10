#include "editor/lexers/mesonlexer.h"

MesonLexer::MesonLexer(QObject *parent)
    : QsciLexerPython(parent)
{
}

const char *MesonLexer::language() const
{
    return "Meson";
}

const char *MesonLexer::keywords(int set) const
{
    if (set == 1)
    {
        return "and break continue elif else endforeach endif false foreach if "
               "in not or true";
    }

    if (set == 2)
    {
        return "add_global_arguments add_global_link_arguments add_languages "
               "assert benchmark both_libraries build_target configuration_data "
               "configure_file custom_target declare_dependency dependency "
               "disabler environment error executable files find_library "
               "find_program generator get_option import include_directories "
               "install_data install_headers install_man install_subdir jar "
               "join_paths library message option project run_command run_target "
               "shared_library static_library subdir subdir_done summary test "
               "vcs_tag warning";
    }

    return QsciLexerPython::keywords(set);
}
