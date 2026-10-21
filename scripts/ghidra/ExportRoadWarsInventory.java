// Read-only inventory export. Run in the already-open, analyzed program.
// @category RoadWars
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SymbolType;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.time.Instant;

public class ExportRoadWarsInventory extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        Path output = args.length > 0 ? Paths.get(args[0]) : askFile("Inventory JSON output", "Save").toPath();
        JsonObject result = new JsonObject();
        result.addProperty("schema_version", 1);
        result.addProperty("program", currentProgram.getName());
        String hash = currentProgram.getExecutableSHA256();
        if (hash == null || hash.length() != 64) throw new IllegalStateException("Program has no executable SHA256");
        result.addProperty("binary_sha256", hash.toLowerCase());
        result.addProperty("exported_at", Instant.now().toString());
        result.addProperty("source", "Read-only Ghidra Java export; run complete_pe_inventory.py to add stable IAT import identities");
        JsonArray functions = new JsonArray();
        var iterator = currentProgram.getFunctionManager().getFunctions(true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            Function function = iterator.next();
            JsonObject row = new JsonObject();
            row.addProperty("entry_point", function.getEntryPoint().toString());
            row.addProperty("end_address", function.getBody().getMaxAddress().toString());
            row.addProperty("name", function.getName());
            row.addProperty("namespace", function.getParentNamespace().getName(true));
            row.addProperty("signature", function.getSignature().toString());
            row.addProperty("is_thunk", function.isThunk());
            functions.add(row);
        }
        // Use the same RTTI descriptor identities as the initial MCP inventory.
        JsonArray classes = new JsonArray();
        var symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext()) {
            monitor.checkCancelled();
            var symbol = symbols.next();
            if (!symbol.getName().endsWith("RTTI_Type_Descriptor")) continue;
            String name = symbol.getParentNamespace().getName(true);
            if (name.equals("Global")) continue;
            JsonObject row = new JsonObject();
            row.addProperty("name", name);
            row.addProperty("rtti0_address", symbol.getAddress().toString());
            classes.add(row);
        }
        result.add("functions", functions);
        result.add("classes", classes);
        result.addProperty("function_count", functions.size());
        result.addProperty("class_count", classes.size());
        Files.writeString(output, new GsonBuilder().setPrettyPrinting().create().toJson(result) + "\n", StandardCharsets.UTF_8);
        println("Exported " + functions.size() + " internal functions and " + classes.size() + " RTTI types");
    }
}
