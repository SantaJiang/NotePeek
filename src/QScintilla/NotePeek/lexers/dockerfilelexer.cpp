#include "NotePeek/lexers/dockerfilelexer.h"

DockerfileLexer::DockerfileLexer(QObject *parent)
    : SimpleSyntaxLexer("Dockerfile",
                        "#",
                        "add arg cmd copy entrypoint env expose from healthcheck label maintainer onbuild run shell stopsignal user volume workdir",
                        "",
                        "",
                        "[]{}()=:+-*/!<>|&.,;\\",
                        true,
                        ShellVariables,
                        true,
                        parent)
{
}
