// This module implements the QsciLexerQML class.

#include "Qsci/qscilexerqml.h"


// The ctor.
QsciLexerQML::QsciLexerQML(QObject *parent)
    : QsciLexerJavaScript(parent)
{
}


// The dtor.
QsciLexerQML::~QsciLexerQML()
{
}


// Returns the language name.
const char *QsciLexerQML::language() const
{
    return "QML";
}


// Returns the set of keywords.
const char *QsciLexerQML::keywords(int set) const
{
    if (set == 1)
        return
            "as break case catch class const continue debugger default delete "
            "do else enum export extends false finally for function if import "
            "in instanceof let new null of return signal switch this throw true "
            "try typeof var void while with yield";

    if (set == 2)
        return
            "alias bool color date double enumeration font int list matrix4x4 "
            "point property quaternion real rect required string url var vector2d "
            "vector3d vector4d readonly default final id on parent children states "
            "transitions anchors Layout model delegate index currentIndex currentItem "
            "contentItem data resources ApplicationWindow Window Item Rectangle Text "
            "TextInput TextEdit Image BorderImage AnimatedImage Button CheckBox "
            "ComboBox Label Menu MenuBar Popup RadioButton Slider SpinBox Switch "
            "TextArea ToolBar ToolButton Dialog Frame GroupBox Page StackView TabBar "
            "SwipeView ScrollView SplitView GridView ListView PathView TableView "
            "TreeView Column Row Grid Flow Repeater Loader Component Connections "
            "Binding State Transition Behavior PropertyAnimation NumberAnimation "
            "ColorAnimation SequentialAnimation ParallelAnimation PauseAnimation "
            "SpringAnimation SmoothedAnimation ScriptAction PropertyAction Timer "
            "MouseArea TapHandler DragHandler PinchHandler HoverHandler Keys FocusScope "
            "Canvas ShaderEffect ShaderEffectSource Flickable QtObject ListModel "
            "ListElement VisualDataModel XmlListModel Settings Shortcut Action Qt";

    return QsciLexerCPP::keywords(set);
}


// Returns the user name of a style.
QString QsciLexerQML::description(int style) const
{
    if (style == KeywordSet2)
        return tr("QML keyword and type");

    return QsciLexerJavaScript::description(style);
}
