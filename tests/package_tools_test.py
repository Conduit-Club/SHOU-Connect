"""Exercise pruning and archive selection on synthetic packages, never a real SDK."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location(
    "package_tools", Path(__file__).resolve().parents[1] / "scripts/package_tools.py")
package_tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package_tools)


class PackageToolsTest(unittest.TestCase):
    def test_preserves_runtime_and_selected_languages(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            keep = ["resources/qtwebengine_devtools_resources.pak", "resources/icudtl.dat",
                    "Qt6Quick.dll", "opengl32sw.dll", "tls/qschannelbackend.dll",
                    "translations/qtwebengine_locales/en-US.pak",
                    "translations/qtwebengine_locales/zh-CN.pak",
                    "translations/qtwebengine_locales/zh-TW.pak",
                    "translations/qtbase_zh_CN.qm", "translations/qt_help_zh_TW.qm",
                    "translations/qtbase_en.qm",
                    "Frameworks/WebEngine/Resources/en.lproj/locale.pak",
                    "Frameworks/WebEngine/Resources/zh_CN.lproj/locale.pak"]
            remove = ["translations/qtbase_de.qm", "translations/qtwebengine_locales/de.pak",
                      "qmltooling/debugger.dll", "app.pdb",
                      "Frameworks/WebEngine/Resources/fr.lproj/locale.pak",
                      "app.dSYM/Contents/Resources/DWARF/app"]
            for name in keep + remove:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"test")
            result = package_tools.prune(root)
            self.assertEqual(result["before_bytes"], 4 * (len(keep) + len(remove)))
            self.assertEqual(result["after_bytes"], 4 * len(keep))
            self.assertTrue(all((root / name).exists() for name in keep))
            self.assertTrue(all(not (root / name).exists() for name in remove))
            self.assertEqual(package_tools.prune(root)["removed"], [])
            report = package_tools.report(root)
            self.assertEqual(report["total_bytes"], result["after_bytes"])
            self.assertEqual(len(report["files"]), len(keep))

    def test_extracts_only_core_and_ignores_zip_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "source.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("nested/zju-connect.exe", b"core")
                bundle.writestr("../../escape.txt", b"unwanted")
                bundle.writestr("README.md", b"unwanted")
            output = root / "output"
            output.mkdir()
            with patch.object(package_tools.urllib.request, "urlretrieve",
                              side_effect=lambda url, target: package_tools.shutil.copyfile(archive, target)):
                package_tools.fetch_core(output, "windows", "amd64", False)
            self.assertEqual([p.name for p in output.iterdir()], ["zju-connect.exe"])
            self.assertEqual((output / "zju-connect.exe").read_bytes(), b"core")

    def test_rejects_duplicate_core_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "source.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("a/zju-connect", b"one")
                bundle.writestr("b/zju-connect", b"two")
            with patch.object(package_tools.urllib.request, "urlretrieve",
                              side_effect=lambda url, target: package_tools.shutil.copyfile(archive, target)):
                with self.assertRaises(RuntimeError):
                    package_tools.fetch_core(root, "linux", "amd64", False)


if __name__ == "__main__":
    unittest.main()
