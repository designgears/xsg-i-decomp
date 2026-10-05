// Post-analysis: write Ghidra's decompilation of every function to <outdir>/<name>.c.
//   args: <outdir>
// Each pointer parameter Ghidra has no struct type for gets a recovered layout
// (FillOutStructure: every offset the function, and the callees it passes the
// pointer to, load or store) in a comment at the top of the file.
import ghidra.app.decompiler.*;
import ghidra.app.decompiler.util.FillOutStructureHelper;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        Path out = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(out);
        DecompInterface d = new DecompInterface();
        d.toggleCCode(true);
        d.openProgram(currentProgram);
        FillOutStructureHelper fill = new FillOutStructureHelper(currentProgram, monitor);
        int ok = 0, bad = 0, layouts = 0;
        java.util.Set<String> written = new java.util.HashSet<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) break;
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            if ((r == null || !r.decompileCompleted()) && f.getParameterCount() > 0) {
                // an imported signature the EE register mapping cannot cover
                // ("Join address range not covered"): drop it and let Ghidra infer
                try {
                    f.updateFunction(null, null, java.util.List.of(),
                        Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.DEFAULT);
                    r = d.decompileFunction(f, 60, monitor);
                } catch (Exception ignored) { }
            }
            if (r == null || !r.decompileCompleted()) {
                bad++;
                println("ExportDecomp: failed " + f.getName() + ": " + (r == null ? "null" : r.getErrorMessage().strip()));
                continue;
            }
            StringBuilder c = new StringBuilder("/* ghidra " + f.getEntryPoint()
                + " (reference only: flow/types hint, not matching C) */\n");
            String lay = layouts(fill, f, r.getHighFunction(), d);
            if (!lay.isEmpty()) { c.append(lay); layouts++; }
            c.append(r.getDecompiledFunction().getC());
            String fname = f.getName().replace('/', '_');
            if (!written.add(fname)) {          // two functions, one name: keep both
                println("ExportDecomp: duplicate name " + fname + " at " + f.getEntryPoint());
                fname = fname + "_" + f.getEntryPoint();
            }
            Files.writeString(out.resolve(fname + ".c"), c.toString());
            ok++;
        }
        d.dispose();
        println("ExportDecomp: " + ok + " written, " + bad + " failed, " + layouts + " with struct layouts");
    }

    private String layouts(FillOutStructureHelper fill, Function f, HighFunction hf, DecompInterface d) {
        if (hf == null) return "";
        StringBuilder sb = new StringBuilder();
        LocalSymbolMap lsm = hf.getLocalSymbolMap();
        for (int i = 0; i < lsm.getNumParams(); i++) {
            HighSymbol ps = lsm.getParamSymbol(i);
            HighVariable hv = ps.getHighVariable();
            if (hv == null) continue;
            DataType t = ps.getDataType();
            if (t instanceof Pointer p && p.getDataType() instanceof Composite) continue;  // already typed
            if (t instanceof Pointer || t.getLength() == 4) {
                try {
                    Structure s = fill.processStructure(hv, f, true, false, d);
                    if (s == null || s.getNumDefinedComponents() == 0) continue;
                    sb.append("/* recovered layout of *").append(ps.getName())
                      .append(" (accesses in this function and callees; size >= 0x")
                      .append(Integer.toHexString(s.getLength())).append("):\n");
                    for (DataTypeComponent comp : s.getDefinedComponents()) {
                        sb.append(String.format("     0x%03X  %-12s (%d bytes)%n", comp.getOffset(),
                            comp.getDataType().getDisplayName(), comp.getLength()));
                    }
                    sb.append(" */\n");
                } catch (Exception ignored) { }
            }
        }
        return sb.toString();
    }
}
