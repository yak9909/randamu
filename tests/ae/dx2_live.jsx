// DupliX 2: an edit made in the editor shows in the next render without purging AE's caches (the user, 11/29: "no
// change made in the editor shows until a Macro is touched in the Effect Controls; changing Copy Scale then does not
// change the layout either"). Steps (run_dx2_live.ps1 sends them to one AE, the idle hook runs between them):
//   setup: a comp with DupliX 2 (the default 5 x 5 grid), frame 0 rendered (AE caches it), then Random Seed = 5:
//          the effect queues dx2_live_script.txt's script (Count X = 3: a 3 x 5 grid) for the idle hook
//   edited: frame 0 again, no purge (the dump must show 15 elements)
//   macro: Copy Scale (Macro 3) = 50, frame 0 again, no purge (15 elements, copy scale 0.5)
(function () {
    var outDir = $.getenv("DUPLIX_TEST_DIR");
    var logFile = new File(outDir + "/jsx.log");
    logFile.encoding = "UTF-8";
    function log(s) { logFile.open("a"); logFile.writeln(s); logFile.close(); }
    var stepFile = new File(outDir + "/step.txt");
    stepFile.open("r");
    var step = stepFile.readln().replace(/^﻿/, "").replace(/\s+$/, "");
    stepFile.close();
    function done() {
        var f = new File(outDir + "/step_done_" + step + ".txt");
        f.open("w"); f.writeln("ok"); f.close();
    }
    function png(comp, name) {
        var f = new File(outDir + "/" + name + ".png");
        comp.saveFrameToPng(0, f);
        for (var k = 0; k < 100 && !f.exists; k++) $.sleep(100);
        log("png " + name + " exists=" + f.exists);
    }
    function find(name) {
        for (var i = 1; i <= app.project.numItems; i++)
            if (app.project.item(i).name == name) return app.project.item(i);
        return null;
    }
    function dx2(id) { return "DupliX2-" + ("000" + id).slice(-4); }
    try {
        app.beginSuppressDialogs();
        if (step == "setup") {
            app.newProject();
            var c = app.project.items.addComp("L", 400, 300, 1, 1, 30);
            var s = c.layers.addSolid([1, 0, 0], "S", 40, 40, 1, 1);
            var fx = s.property("ADBE Effect Parade").addProperty("DupliX2");
            c.openInViewer();  // as the user works: the comp shown (its viewer renders by itself)
            png(c, "l_before");
            fx.property(dx2(3)).setValue(5);  // Random Seed: the editor script for seed 5
            log("setup ok");
        } else if (step == "edited") {
            png(find("L"), "l_edited");
            log("edited ok");
        } else if (step == "macro") {
            var fx2 = find("L").layer(1).property("ADBE Effect Parade").property(1);
            fx2.property(dx2(7)).setValue(50);  // Macro 3 = Copy Scale
            png(find("L"), "l_macro");
            log("macro ok");
        }
    } catch (e) {
        log("EXCEPTION " + String(e.message) + " (line " + String(e.line) + ")");
    }
    done();
})();
