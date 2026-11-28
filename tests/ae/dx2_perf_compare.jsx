// DupliX 2 Fast (2D) against DupliX (version 1) Fast (2D), the same copies (the user, 11/29: "changing Copy Scale
// makes it very slow, spreading it out too; far slower than DupliX's fast mode"). 1920x1080 comp, a 1920x1080 solid.
// DupliX 2: the default grid 5 x 5 (Int Macro 1 = Count 5), Macro 2 = Spacing, Macro 3 = Copy Scale.
// DupliX: 25 copies, scattered over +-X / +-Y (the grid's extent), scale min = max = Copy Scale.
// Each case: frames 0..2 (caches purged before each), the time saveFrameToPng took. The plugins log their own
// PreRender / SmartRender times (plugin.log).
(function () {
    var outDir = $.getenv("DUPLIX_TEST_DIR");
    var logFile = new File(outDir + "/jsx.log");
    logFile.encoding = "UTF-8";
    function log(s) { logFile.open("a"); logFile.writeln(s); logFile.close(); }
    function dx(id) { return "DupliX-" + ("000" + id).slice(-4); }
    try {
        app.beginSuppressDialogs();
        app.newProject();
        // DUPLIX_PERF_GPU=1: Mercury GPU (CUDA), as an artist's project usually is
        if ($.getenv("DUPLIX_PERF_GPU") == "1") {
            try { app.project.gpuAccelType = GpuAccelType.CUDA; log("gpu: CUDA"); } catch (eg) { log("gpu: " + eg.message); }
        }
        log("bpc " + app.project.bitsPerChannel);
        var cases = [
            {spacing: 100, scale: 100},
            {spacing: 100, scale: 25},
            {spacing: 100, scale: 300},
            {spacing: 1000, scale: 100},
            {spacing: 1000, scale: 300}
        ];
        for (var v = 0; v < 2; v++) {
            for (var i = 0; i < cases.length; i++) {
                var cs = cases[i];
                var name = (v ? "v1" : "v2") + "_sp" + cs.spacing + "_sc" + cs.scale;
                var c = app.project.items.addComp(name, 1920, 1080, 1, 1, 30);
                var s = c.layers.addSolid([1, 0.5, 0], "S", 1920, 1080, 1, 1);
                var fx;
                if (v == 0) {
                    fx = s.property("ADBE Effect Parade").addProperty("DupliX2");
                    fx.property("DupliX2-0006").setValue(cs.spacing);  // Macro 2 = Spacing
                    fx.property("DupliX2-0007").setValue(cs.scale);    // Macro 3 = Copy Scale
                } else {
                    fx = s.property("ADBE Effect Parade").addProperty("DupliX");
                    fx.property("DupliX-0037").setValue(2);            // Fast (2D)
                    fx.property("DupliX-0002").setValue(25);
                    fx.property("DupliX-0003").setValue(1);
                    fx.property(dx(4)).setValue(cs.spacing * 2);       // the grid's extent (+-2 cells)
                    fx.property(dx(5)).setValue(cs.spacing * 2);
                    fx.property(dx(6)).setValue(0);
                    fx.property("DupliX-0010").setValue(cs.scale);
                    fx.property("DupliX-0011").setValue(cs.scale);
                }
                var ms = [];
                for (var k = 0; k < 3; k++) {
                    app.purge(PurgeTarget.ALL_CACHES);
                    var f = new File(outDir + "/" + name + "_" + k + ".png");
                    var t0 = new Date().getTime();
                    c.saveFrameToPng(k / 30, f);
                    for (var n = 0; n < 400 && !f.exists; n++) $.sleep(25);
                    ms.push(new Date().getTime() - t0);
                }
                log(name + ": " + ms.join(" ") + " ms");
            }
        }
        log("done");
    } catch (e) {
        log("EXCEPTION " + String(e.message) + " (line " + String(e.line) + ")");
    }
})();
