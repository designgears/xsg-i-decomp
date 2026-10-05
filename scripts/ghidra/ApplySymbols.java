// Pre- and post-analysis (export.sh runs it twice: analysis may run one function
// on into the next, the second pass splits them again): name and create every function from a config/symbols/<unit>.txt file.
//   args: <symbols.txt>
// Lines: `name = 0xADDR; // type:func size:0xN` (type:func -> function, else a label).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolType;
import java.nio.file.*;
import java.util.regex.*;

public class ApplySymbols extends GhidraScript {
    private static final Pattern LINE = Pattern.compile("^\\s*([A-Za-z_.$][\\w.$]*)\\s*=\\s*0x([0-9A-Fa-f]+);(.*)$");

    @Override
    public void run() throws Exception {
        int funcs = 0, labels = 0;
        var lines = Files.readAllLines(Paths.get(getScriptArgs()[0]));
        // The ELF's own symbols get in the way: a `gcc2_compiled.` label opens every
        // TU, and statics of one name sit at several addresses. Drop those whose
        // name the symbols file gives to another address (or that are gcc2_compiled.).
        var want = new java.util.HashMap<String, Long>();
        for (String line : lines) {
            Matcher m = LINE.matcher(line);
            if (m.matches()) want.putIfAbsent(m.group(1), Long.parseLong(m.group(2), 16));
        }
        var st = currentProgram.getSymbolTable();
        var doomed = new java.util.ArrayList<Symbol>();
        for (Symbol s : st.getAllSymbols(false)) {
            Long w = want.get(s.getName());
            if (s.getName().startsWith("gcc2_compiled") || (w != null && w != s.getAddress().getOffset()))
                doomed.add(s);
        }
        for (Symbol s : doomed) {
            if (s.getSymbolType() == SymbolType.FUNCTION) {
                var fn = getFunctionAt(s.getAddress());
                if (fn != null) fn.setName("FUN_" + s.getAddress(), SourceType.DEFAULT);
            } else {
                s.delete();
            }
        }
        for (String line : lines) {
            Matcher m = LINE.matcher(line);
            if (!m.matches()) continue;
            Address a = toAddr(Long.parseLong(m.group(2), 16));
            if (currentProgram.getMemory().getBlock(a) == null) continue;   // other image's address
            String name = m.group(1);
            try {
            if (m.group(3).contains("type:func")) {
                disassemble(a);
                var f = getFunctionAt(a);
                var outer = f == null ? getFunctionContaining(a) : null;
                if (outer != null) {            // analysis ran on into this function: split it off
                    var body = outer.getBody().subtract(new AddressSet(a, outer.getBody().getMaxAddress()));
                    outer.setBody(body);
                }
                if (f == null) f = createFunction(a, name);
                if (f != null) {
                    Symbol same = st.getGlobalSymbol(name, a);   // e.g. the ELF's label
                    if (same != null && !same.isPrimary()) same.delete();
                    f.setName(name, SourceType.IMPORTED);
                    funcs++;
                }
            } else {
                createLabel(a, name, true, SourceType.IMPORTED);
                labels++;
            }
            } catch (Exception e) {     // e.g. a second name for one address: keep the first
                if (getSymbolAt(a) != null) createLabel(a, name, false, SourceType.IMPORTED);
            }
        }
        println("ApplySymbols: " + funcs + " functions, " + labels + " labels");
    }
}
