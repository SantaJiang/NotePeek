function Component() {
    // 空构造函数
}

Component.prototype.createOperations = function() {
    component.createOperations();

    var os = installer.value("os");

    // Windows平台
    if (os === "win") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/notepeek.exe",
            "@StartMenuDir@/NotePeek.lnk",
            "workingDirectory=@TargetDir@",
            "description=Open Application");

        component.addOperation("CreateShortcut",
            "@TargetDir@/notepeek.exe",
            "@DesktopDir@/NotePeek.lnk",
            "workingDirectory=@TargetDir@",
            "description=Open Application");
    }
    // Linux平台
    else if (os === "x11" || os === "wayland") {
        component.addOperation("CreateDesktopEntry",
            "@TargetDir@/notepeek",
            "@DesktopDir@/NotePeek.desktop",
            "Name=NotePeek",
            "Exec=@TargetDir@/notepeek",
            "Icon=@TargetDir@/icon.png",
            "Terminal=false",
            "Type=Application",
            "Categories=Utility;");

        component.addOperation("CreateDesktopEntry",
            "@TargetDir@/notepeek",
            "@HomeDir@/.local/share/applications/NotePeek.desktop",
            "Name=NotePeek",
            "Exec=@TargetDir@/notepeek",
            "Icon=@TargetDir@/icon.png",
            "Terminal=false",
            "Type=Application",
            "Categories=Utility;");
    }
    // macOS平台
    else if (os === "macx") {
        component.addOperation("CreateSymlink",
            "@TargetDir@/NotePeek.app",
            "@HomeDir@/Applications/NotePeek.app");
    }
}
