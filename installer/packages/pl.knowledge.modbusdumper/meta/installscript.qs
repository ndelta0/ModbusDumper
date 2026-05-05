function Component() {
}

Component.prototype.createOperations = function() {
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/bin/ModbusDumper.exe",
            "@StartMenuDir@/ModbusDumper.lnk",
            "workingDirectory=@TargetDir@/bin",
            "description=ModbusDumper"
        );

        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/bin/ModbusDumper.exe",
            "@DesktopDir@/ModbusDumper.lnk",
            "workingDirectory=@TargetDir@/bin",
            "description=ModbusDumper"
        );
    }
}