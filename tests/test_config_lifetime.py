"""Execute production config readers across provider registrations.

Retired allocations stay alive to detect stale reads deterministically.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
LOOKUP = re.compile(
    r'(?P<declaration>(?:static\s+)?auto[^\n;]*?\b(?P<name>P[A-Z0-9_]+)\s*='
    r'\s*\(Hyprlang::(?P<kind>INT|FLOAT|STRING)[^;]*?'
    r'getConfigValue\(PHANDLE,\s*"(?P<key>[^"]+)"\)->getDataStaticPtr\(\);)'
)


class ConfigLifetimeTests(unittest.TestCase):
    def test_readers_observe_reregistered_values_without_reloading_the_image(self):
        readers, keys = [], {}
        for filename in ("Overview.cpp", "OverviewRender.cpp", "OverviewInteraction.cpp"):
            matches = list(LOOKUP.finditer((ROOT / "src" / filename).read_text()))
            self.assertTrue(matches, filename + " must contribute its production lookups")
            for match in matches:
                name, kind, key = (match[group] for group in ("name", "kind", "key"))
                self.assertEqual(keys.setdefault(key, kind), kind)
                value = "std::string{*" + name + "}" if kind == "STRING" else "std::to_string(**" + name + ")"
                readers.append("std::string read" + str(len(readers)) + "() { " + match["declaration"] + " return " + value + "; }")

        source = r'''
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace Hyprlang { using INT = int64_t; using FLOAT = float; using STRING = const char*; }
void* PHANDLE = nullptr;
struct Value {
    std::string kind;
    Hyprlang::INT integer = 0;
    Hyprlang::INT* integerPtr = &integer;
    Hyprlang::FLOAT floating = 0;
    Hyprlang::FLOAT* floatingPtr = &floating;
    std::string storage;
    Hyprlang::STRING string = nullptr;
    void set(int generation) {
        integer = generation;
        floating = generation;
        storage = std::string(128, 'a' + generation);
        string = storage.c_str();
    }
    void* getDataStaticPtr() {
        if (kind == "STRING") return &string;
        if (kind == "FLOAT") return &floatingPtr;
        return &integerPtr;
    }
};
namespace HyprlandAPI {
std::map<std::string, std::unique_ptr<Value>> current;
Value* getConfigValue(void*, const std::string& name) { return current.at(name).get(); }
}
'''
        source += "\n".join(readers)
        source += "\nint main() {\nstd::vector<std::unique_ptr<Value>> retired;\n"
        source += "for (int generation = 1; generation <= 5; ++generation) {\n"
        for key, kind in keys.items():
            source += '{ auto value = std::make_unique<Value>(); value->kind = "' + kind + '"; value->set(generation); HyprlandAPI::current["' + key + '"] = std::move(value); }\n'
        index = 0
        for filename in ("Overview.cpp", "OverviewRender.cpp", "OverviewInteraction.cpp"):
            for match in LOOKUP.finditer((ROOT / "src" / filename).read_text()):
                kind = match["kind"]
                expected = ("std::string(128, 'a' + generation)" if kind == "STRING" else
                            "std::to_string(static_cast<float>(generation))" if kind == "FLOAT" else "std::to_string(generation)")
                source += 'if (read' + str(index) + '() != ' + expected + ') { std::cerr << "stale configuration: ' + filename + ':' + match["name"] + '\\n"; return 1; }\n'
                index += 1
        source += r'''
// Retire the registration without unloading the plugin's static storage.
for (auto& [key, value] : HyprlandAPI::current) retired.push_back(std::move(value));
HyprlandAPI::current.clear();
}
std::cout << "configuration lifetime regression passed\n";
}
'''
        with tempfile.TemporaryDirectory(prefix="hyprexpo-config-lifetime-") as temporary:
            cpp = Path(temporary) / "test.cpp"
            binary = Path(temporary) / "test"
            cpp.write_text(source)
            compile_result = subprocess.run([*shlex.split(os.environ.get("CXX", "c++")), "-std=c++23", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("configuration lifetime regression passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
