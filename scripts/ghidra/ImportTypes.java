// Post-analysis: import the project's own C declarations so Ghidra decompiles
// with real types. args: <dir of <tu>.h files> (build/ghidra/types/<unit>, written
// by scripts/auto_decomp/proto_index.py). Each header is parsed on its own; its
// prototypes are applied to the functions of that name and its extern objects'
// types (the `__gvar_<name>` typedefs proto_index.py adds) to the data at that symbol.
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.File;
import java.nio.file.*;
import java.util.*;

public class ImportTypes extends GhidraScript {
    @Override
    public void run() throws Exception {
        File dir = new File(getScriptArgs()[0]);
        File[] files = dir.listFiles((d, n) -> n.endsWith(".h"));
        if (files == null) { println("ImportTypes: no headers in " + dir); return; }
        Arrays.sort(files);
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        int parsed = 0, failed = 0, sigs = 0, globals = 0;
        for (File f : files) {
            CParser p = new CParser(dtm, true, null);
            try {
                p.parse(Files.newInputStream(f.toPath()));
            } catch (Throwable e) {
                failed++;
                continue;
            }
            parsed++;
            for (Map.Entry<String, DataType> e : p.getFunctions().entrySet()) {
                if (!(e.getValue() instanceof FunctionDefinition fd)) continue;
                for (Function fn : getGlobalFunctions(e.getKey())) {
                    try {
                        if (new ApplyFunctionSignatureCmd(fn.getEntryPoint(), fd, SourceType.IMPORTED)
                                .applyTo(currentProgram, monitor)) sigs++;
                    } catch (Exception ignored) { }
                }
            }
            for (Map.Entry<String, DataType> e : p.getTypes().entrySet()) {
                if (!e.getKey().startsWith("__gvar_")) continue;      // see proto_index.py
                DataType t = e.getValue();
                if (t instanceof TypeDef td) t = td.getDataType();
                if (t instanceof FunctionDefinition || t.getLength() <= 0) continue;
                for (Symbol s : currentProgram.getSymbolTable().getGlobalSymbols(e.getKey().substring(7))) {
                    if (s.getSymbolType() != SymbolType.LABEL) continue;
                    try {
                        var a = s.getAddress();
                        clearListing(a, a.add(t.getLength() - 1));
                        createData(a, t);
                        globals++;
                    } catch (Exception ignored) { }
                }
            }
        }
                println("ImportTypes: " + parsed + " headers (" + failed + " failed), "
                + sigs + " signatures, " + globals + " typed globals");
    }
}
