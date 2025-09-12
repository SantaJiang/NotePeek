!win32:VERSION = 15.2.1

TEMPLATE = lib
CONFIG += qt warn_off thread exceptions hide_symbols
DEFINES += QSCINTILLA_LIBRARY

CONFIG(release, debug|release) {
    DESTDIR = $$PWD/../Bin/Release
} else:CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../Bin/Debug
}

# 修改TARGET名称以匹配项目规范
CONFIG(debug, debug|release) {
    mac: {
        TARGET = qscintilla2d

        # 添加 macOS 特定的动态库设置
        QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/lib$${TARGET}.dylib
    } else {
        TARGET = qscintilla2
    }
} else {
    TARGET = qscintilla2
    mac: {
        # 添加 macOS 特定的动态库设置
        QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/lib$${TARGET}.dylib
    }
}

macx:!CONFIG(staticlib) {
    LIB_TARGET = lib$${TARGET}.dylib
    QMAKE_POST_LINK = install_name_tool -id @rpath/$${LIB_TARGET} $$DESTDIR/$${LIB_TARGET}
}

INCLUDEPATH += . ./scintilla/include ./scintilla/lexlib ./scintilla/src

!CONFIG(staticlib) {
    DEFINES += QSCINTILLA_MAKE_DLL

    # Comment this in to build a dynamic library supporting multiple
    # architectures on macOS.
    #QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
}
DEFINES += SCINTILLA_QT SCI_LEXER INCLUDE_DEPRECATED_FEATURES

QT += widgets
!ios:QT += printsupport
macx:lessThan(QT_MAJOR_VERSION, 6) {
    QT += macextras
}

# For old versions of GCC.
unix:!macx {
    CONFIG += c++11
}

HEADERS = \
    ./Qsci/qsciglobal.h \
    ./Qsci/qsciscintilla.h \
    ./Qsci/qsciscintillabase.h \
    ./Qsci/qsciabstractapis.h \
    ./Qsci/qsciapis.h \
    ./Qsci/qscicommand.h \
    ./Qsci/qscicommandset.h \
    ./Qsci/qscidocument.h \
    ./Qsci/qscilexer.h \
    ./Qsci/qscilexerasm.h \
    ./Qsci/qscilexeravs.h \
    ./Qsci/qscilexerbash.h \
    ./Qsci/qscilexerbatch.h \
    ./Qsci/qscilexercmake.h \
    ./Qsci/qscilexercoffeescript.h \
    ./Qsci/qscilexercpp.h \
    ./Qsci/qscilexercsharp.h \
    ./Qsci/qscilexercss.h \
    ./Qsci/qscilexercustom.h \
    ./Qsci/qscilexerd.h \
    ./Qsci/qscilexerdiff.h \
    ./Qsci/qscilexeredifact.h \
    ./Qsci/qscilexerfortran.h \
    ./Qsci/qscilexerfortran77.h \
    ./Qsci/qscilexerhex.h \
    ./Qsci/qscilexerhtml.h \
    ./Qsci/qscilexeridl.h \
    ./Qsci/qscilexerintelhex.h \
    ./Qsci/qscilexerjava.h \
    ./Qsci/qscilexerjavascript.h \
    ./Qsci/qscilexerjson.h \
    ./Qsci/qscilexerlua.h \
    ./Qsci/qscilexermakefile.h \
    ./Qsci/qscilexermarkdown.h \
    ./Qsci/qscilexermasm.h \
    ./Qsci/qscilexermatlab.h \
    ./Qsci/qscilexernasm.h \
    ./Qsci/qscilexeroctave.h \
    ./Qsci/qscilexerpascal.h \
    ./Qsci/qscilexerperl.h \
    ./Qsci/qscilexerpostscript.h \
    ./Qsci/qscilexerpo.h \
    ./Qsci/qscilexerpov.h \
    ./Qsci/qscilexerproperties.h \
    ./Qsci/qscilexerpython.h \
    ./Qsci/qscilexerruby.h \
    ./Qsci/qscilexerspice.h \
    ./Qsci/qscilexersql.h \
    ./Qsci/qscilexersrec.h \
    ./Qsci/qscilexertcl.h \
    ./Qsci/qscilexertekhex.h \
    ./Qsci/qscilexertex.h \
    ./Qsci/qscilexerverilog.h \
    ./Qsci/qscilexervhdl.h \
    ./Qsci/qscilexerxml.h \
    ./Qsci/qscilexeryaml.h \
    ./Qsci/qscimacro.h \
    ./Qsci/qscistyle.h \
    ./Qsci/qscistyledtext.h \
    ListBoxQt.h \
    SciAccessibility.h \
    SciClasses.h \
    ScintillaQt.h \
    ./scintilla/include/ILexer.h \
    ./scintilla/include/ILoader.h \
    ./scintilla/include/Platform.h \
    ./scintilla/include/Sci_Position.h \
    ./scintilla/include/SciLexer.h \
    ./scintilla/include/Scintilla.h \
    ./scintilla/include/ScintillaWidget.h \
    ./scintilla/lexlib/Accessor.h \
    ./scintilla/lexlib/CharacterCategory.h \
    ./scintilla/lexlib/CharacterSet.h \
    ./scintilla/lexlib/DefaultLexer.h \
    ./scintilla/lexlib/LexAccessor.h \
    ./scintilla/lexlib/LexerBase.h \
    ./scintilla/lexlib/LexerModule.h \
    ./scintilla/lexlib/LexerNoExceptions.h \
    ./scintilla/lexlib/LexerSimple.h \
    ./scintilla/lexlib/OptionSet.h \
    ./scintilla/lexlib/PropSetSimple.h \
    ./scintilla/lexlib/SparseState.h \
    ./scintilla/lexlib/StringCopy.h \
    ./scintilla/lexlib/StyleContext.h \
    ./scintilla/lexlib/SubStyles.h \
    ./scintilla/lexlib/WordList.h \
    ./scintilla/src/AutoComplete.h \
    ./scintilla/src/CallTip.h \
    ./scintilla/src/CaseConvert.h \
    ./scintilla/src/CaseFolder.h \
    ./scintilla/src/Catalogue.h \
    ./scintilla/src/CellBuffer.h \
    ./scintilla/src/CharClassify.h \
    ./scintilla/src/ContractionState.h \
    ./scintilla/src/DBCS.h \
    ./scintilla/src/Decoration.h \
    ./scintilla/src/Document.h \
    ./scintilla/src/EditModel.h \
    ./scintilla/src/Editor.h \
    ./scintilla/src/EditView.h \
    ./scintilla/src/ElapsedPeriod.h \
    ./scintilla/src/ExternalLexer.h \
    ./scintilla/src/FontQuality.h \
    ./scintilla/src/Indicator.h \
    ./scintilla/src/IntegerRectangle.h \
    ./scintilla/src/KeyMap.h \
    ./scintilla/src/LineMarker.h \
    ./scintilla/src/MarginView.h \
    ./scintilla/src/Partitioning.h \
    ./scintilla/src/PerLine.h \
    ./scintilla/src/Position.h \
    ./scintilla/src/PositionCache.h \
    ./scintilla/src/RESearch.h \
    ./scintilla/src/RunStyles.h \
    ./scintilla/src/ScintillaBase.h \
    ./scintilla/src/Selection.h \
    ./scintilla/src/SparseVector.h \
    ./scintilla/src/SplitVector.h \
    ./scintilla/src/Style.h \
    ./scintilla/src/UniConversion.h \
    ./scintilla/src/UniqueString.h \
    ./scintilla/src/ViewStyle.h \
    ./scintilla/src/XPM.h

!ios:HEADERS += ./Qsci/qsciprinter.h

SOURCES = \
    qsciscintilla.cpp \
    qsciscintillabase.cpp \
    qsciabstractapis.cpp \
    qsciapis.cpp \
    qscicommand.cpp \
    qscicommandset.cpp \
    qscidocument.cpp \
    qscilexer.cpp \
    qscilexerasm.cpp \
    qscilexeravs.cpp \
    qscilexerbash.cpp \
    qscilexerbatch.cpp \
    qscilexercmake.cpp \
    qscilexercoffeescript.cpp \
    qscilexercpp.cpp \
    qscilexercsharp.cpp \
    qscilexercss.cpp \
    qscilexercustom.cpp \
    qscilexerd.cpp \
    qscilexerdiff.cpp \
    qscilexeredifact.cpp \
    qscilexerfortran.cpp \
    qscilexerfortran77.cpp \
    qscilexerhex.cpp \
    qscilexerhtml.cpp \
    qscilexeridl.cpp \
    qscilexerintelhex.cpp \
    qscilexerjava.cpp \
    qscilexerjavascript.cpp \
    qscilexerjson.cpp \
    qscilexerlua.cpp \
    qscilexermakefile.cpp \
    qscilexermarkdown.cpp \
    qscilexermasm.cpp \
    qscilexermatlab.cpp \
    qscilexernasm.cpp \
    qscilexeroctave.cpp \
    qscilexerpascal.cpp \
    qscilexerperl.cpp \
    qscilexerpostscript.cpp \
    qscilexerpo.cpp \
    qscilexerpov.cpp \
    qscilexerproperties.cpp \
    qscilexerpython.cpp \
    qscilexerruby.cpp \
    qscilexerspice.cpp \
    qscilexersql.cpp \
    qscilexersrec.cpp \
    qscilexertcl.cpp \
    qscilexertekhex.cpp \
    qscilexertex.cpp \
    qscilexerverilog.cpp \
    qscilexervhdl.cpp \
    qscilexerxml.cpp \
    qscilexeryaml.cpp \
    qscimacro.cpp \
    qscistyle.cpp \
    qscistyledtext.cpp \
    InputMethod.cpp \
    ListBoxQt.cpp \
    MacPasteboardMime.cpp \
    PlatQt.cpp \
    SciAccessibility.cpp \
    SciClasses.cpp \
    ScintillaQt.cpp \
    ./scintilla/lexers/LexA68k.cpp \
    ./scintilla/lexers/LexAPDL.cpp \
    ./scintilla/lexers/LexASY.cpp \
    ./scintilla/lexers/LexAU3.cpp \
    ./scintilla/lexers/LexAVE.cpp \
    ./scintilla/lexers/LexAVS.cpp \
    ./scintilla/lexers/LexAbaqus.cpp \
    ./scintilla/lexers/LexAda.cpp \
    ./scintilla/lexers/LexAsm.cpp \
    ./scintilla/lexers/LexAsn1.cpp \
    ./scintilla/lexers/LexBaan.cpp \
    ./scintilla/lexers/LexBash.cpp \
    ./scintilla/lexers/LexBasic.cpp \
    ./scintilla/lexers/LexBatch.cpp \
    ./scintilla/lexers/LexBibTeX.cpp \
    ./scintilla/lexers/LexBullant.cpp \
    ./scintilla/lexers/LexCLW.cpp \
    ./scintilla/lexers/LexCOBOL.cpp \
    ./scintilla/lexers/LexCPP.cpp \
    ./scintilla/lexers/LexCSS.cpp \
    ./scintilla/lexers/LexCaml.cpp \
    ./scintilla/lexers/LexCmake.cpp \
    ./scintilla/lexers/LexCoffeeScript.cpp \
    ./scintilla/lexers/LexConf.cpp \
    ./scintilla/lexers/LexCrontab.cpp \
    ./scintilla/lexers/LexCsound.cpp \
    ./scintilla/lexers/LexD.cpp \
    ./scintilla/lexers/LexDMAP.cpp \
    ./scintilla/lexers/LexDMIS.cpp \
    ./scintilla/lexers/LexDiff.cpp \
    ./scintilla/lexers/LexECL.cpp \
    ./scintilla/lexers/LexEDIFACT.cpp \
    ./scintilla/lexers/LexEScript.cpp \
    ./scintilla/lexers/LexEiffel.cpp \
    ./scintilla/lexers/LexErlang.cpp \
    ./scintilla/lexers/LexErrorList.cpp \
    ./scintilla/lexers/LexFlagship.cpp \
    ./scintilla/lexers/LexForth.cpp \
    ./scintilla/lexers/LexFortran.cpp \
    ./scintilla/lexers/LexGAP.cpp \
    ./scintilla/lexers/LexGui4Cli.cpp \
    ./scintilla/lexers/LexHTML.cpp \
    ./scintilla/lexers/LexHaskell.cpp \
    ./scintilla/lexers/LexHex.cpp \
    ./scintilla/lexers/LexIndent.cpp \
    ./scintilla/lexers/LexInno.cpp \
    ./scintilla/lexers/LexJSON.cpp \
    ./scintilla/lexers/LexKVIrc.cpp \
    ./scintilla/lexers/LexKix.cpp \
    ./scintilla/lexers/LexLaTeX.cpp \
    ./scintilla/lexers/LexLisp.cpp \
    ./scintilla/lexers/LexLout.cpp \
    ./scintilla/lexers/LexLua.cpp \
    ./scintilla/lexers/LexMMIXAL.cpp \
    ./scintilla/lexers/LexMPT.cpp \
    ./scintilla/lexers/LexMSSQL.cpp \
    ./scintilla/lexers/LexMagik.cpp \
    ./scintilla/lexers/LexMake.cpp \
    ./scintilla/lexers/LexMarkdown.cpp \
    ./scintilla/lexers/LexMatlab.cpp \
    ./scintilla/lexers/LexMaxima.cpp \
    ./scintilla/lexers/LexMetapost.cpp \
    ./scintilla/lexers/LexModula.cpp \
    ./scintilla/lexers/LexMySQL.cpp \
    ./scintilla/lexers/LexNimrod.cpp \
    ./scintilla/lexers/LexNsis.cpp \
    ./scintilla/lexers/LexNull.cpp \
    ./scintilla/lexers/LexOScript.cpp \
    ./scintilla/lexers/LexOpal.cpp \
    ./scintilla/lexers/LexPB.cpp \
    ./scintilla/lexers/LexPLM.cpp \
    ./scintilla/lexers/LexPO.cpp \
    ./scintilla/lexers/LexPOV.cpp \
    ./scintilla/lexers/LexPS.cpp \
    ./scintilla/lexers/LexPascal.cpp \
    ./scintilla/lexers/LexPerl.cpp \
    ./scintilla/lexers/LexPowerPro.cpp \
    ./scintilla/lexers/LexPowerShell.cpp \
    ./scintilla/lexers/LexProgress.cpp \
    ./scintilla/lexers/LexProps.cpp \
    ./scintilla/lexers/LexPython.cpp \
    ./scintilla/lexers/LexR.cpp \
    ./scintilla/lexers/LexRebol.cpp \
    ./scintilla/lexers/LexRegistry.cpp \
    ./scintilla/lexers/LexRuby.cpp \
    ./scintilla/lexers/LexRust.cpp \
    ./scintilla/lexers/LexSAS.cpp \
    ./scintilla/lexers/LexSML.cpp \
    ./scintilla/lexers/LexSQL.cpp \
    ./scintilla/lexers/LexSTTXT.cpp \
    ./scintilla/lexers/LexScriptol.cpp \
    ./scintilla/lexers/LexSmalltalk.cpp \
    ./scintilla/lexers/LexSorcus.cpp \
    ./scintilla/lexers/LexSpecman.cpp \
    ./scintilla/lexers/LexSpice.cpp \
    ./scintilla/lexers/LexStata.cpp \
    ./scintilla/lexers/LexTACL.cpp \
    ./scintilla/lexers/LexTADS3.cpp \
    ./scintilla/lexers/LexTAL.cpp \
    ./scintilla/lexers/LexTCL.cpp \
    ./scintilla/lexers/LexTCMD.cpp \
    ./scintilla/lexers/LexTeX.cpp \
    ./scintilla/lexers/LexTxt2tags.cpp \
    ./scintilla/lexers/LexVB.cpp \
    ./scintilla/lexers/LexVHDL.cpp \
    ./scintilla/lexers/LexVerilog.cpp \
    ./scintilla/lexers/LexVisualProlog.cpp \
    ./scintilla/lexers/LexYAML.cpp \
    ./scintilla/lexlib/Accessor.cpp \
    ./scintilla/lexlib/CharacterCategory.cpp \
    ./scintilla/lexlib/CharacterSet.cpp \
    ./scintilla/lexlib/DefaultLexer.cpp \
    ./scintilla/lexlib/LexerBase.cpp \
    ./scintilla/lexlib/LexerModule.cpp \
    ./scintilla/lexlib/LexerNoExceptions.cpp \
    ./scintilla/lexlib/LexerSimple.cpp \
    ./scintilla/lexlib/PropSetSimple.cpp \
    ./scintilla/lexlib/StyleContext.cpp \
    ./scintilla/lexlib/WordList.cpp \
    ./scintilla/src/AutoComplete.cpp \
    ./scintilla/src/CallTip.cpp \
    ./scintilla/src/CaseConvert.cpp \
    ./scintilla/src/CaseFolder.cpp \
    ./scintilla/src/Catalogue.cpp \
    ./scintilla/src/CellBuffer.cpp \
    ./scintilla/src/CharClassify.cpp \
    ./scintilla/src/ContractionState.cpp \
    ./scintilla/src/DBCS.cpp \
    ./scintilla/src/Decoration.cpp \
    ./scintilla/src/Document.cpp \
    ./scintilla/src/EditModel.cpp \
    ./scintilla/src/Editor.cpp \
    ./scintilla/src/EditView.cpp \
    ./scintilla/src/ExternalLexer.cpp \
    ./scintilla/src/Indicator.cpp \
    ./scintilla/src/KeyMap.cpp \
    ./scintilla/src/LineMarker.cpp \
    ./scintilla/src/MarginView.cpp \
    ./scintilla/src/PerLine.cpp \
    ./scintilla/src/PositionCache.cpp \
    ./scintilla/src/RESearch.cpp \
    ./scintilla/src/RunStyles.cpp \
    ./scintilla/src/ScintillaBase.cpp \
    ./scintilla/src/Selection.cpp \
    ./scintilla/src/Style.cpp \
    ./scintilla/src/UniConversion.cpp \
    ./scintilla/src/ViewStyle.cpp \
    ./scintilla/src/XPM.cpp

!ios:SOURCES += qsciprinter.cpp

TRANSLATIONS = \
    qscintilla_cs.ts \
    qscintilla_de.ts \
    qscintilla_es.ts \
    qscintilla_fr.ts \
    qscintilla_pt_br.ts

isEmpty(target.path) {
    qnx: target.path = /tmp/notepeek/bin
    else: unix:!android: target.path = /opt/notepeek/bin
    !isEmpty(target.path): INSTALLS += target
}
