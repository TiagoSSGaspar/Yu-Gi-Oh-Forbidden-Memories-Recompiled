#!/usr/bin/env python3
"""Test how tools/pc/check_mod_abi.py fetches a baseline release: only the
package whose sha256 tools/pc/mod_compat.txt pins is unpacked, the package is
kept beside the folder and checked again whenever the folder is used, a
folder without its package is fetched again, and a package holding a name
outside its folder is refused. Synthetic packages, served from a folder of
this test's through file:// URLs; nothing is downloaded or run."""
import hashlib, io, os, pathlib, sys, tarfile, tempfile, unittest, zipfile
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_mod_abi

TAG = "v9.9.9"
TOP = f"yfm-redecomp-{TAG}"
NAMES = {"windows": f"{TOP}-windows.zip", "linux": f"{TOP}-linux.tar.gz"}


def make_zip(path, members):
    with zipfile.ZipFile(path, "w") as package:
        for name, data in members.items():
            package.writestr(name, data)


def make_tar(path, members):
    with tarfile.open(path, "w:gz") as package:
        for name, data in members.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            package.addfile(info, io.BytesIO(data))


def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


class Fetch(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="mod-compat-fetch-")
        base = self.scratch.name
        self.cache, self.server = os.path.join(base, "cache"), os.path.join(base, "server")
        os.makedirs(os.path.join(self.server, TAG))
        for name, value in (("CACHE", self.cache), ("DOWNLOADS", pathlib.Path(self.server).as_uri()),
                            ("print", lambda *arguments, **options: None)):
            patcher = mock.patch.object(check_mod_abi, name, value, create=True)
            patcher.start()
            self.addCleanup(patcher.stop)
        self.addCleanup(self.scratch.cleanup)
        self.downloads = []
        real = check_mod_abi.urllib.request.urlopen

        def counting(url, *arguments, **options):
            self.downloads.append(url)
            return real(url, *arguments, **options)
        patcher = mock.patch.object(check_mod_abi.urllib.request, "urlopen", counting)
        patcher.start()
        self.addCleanup(patcher.stop)

    def publish(self, system="windows", members=None):
        """A package on the test's server: {(TAG, system): its sha256}."""
        members = members or {f"{TOP}/sdk/exports.txt": b"Mod_Name\n", f"{TOP}/mods/a/mod.json": b"{}"}
        path = os.path.join(self.server, TAG, NAMES[system])
        (make_zip if system == "windows" else make_tar)(path, members)
        return {(TAG, system): digest(path)}

    def folder(self, system="windows"):
        return os.path.join(self.cache, TAG, system)

    def unpacked(self, system="windows"):
        return os.path.join(self.folder(system), "verified", TOP)

    def assert_only_kept(self, system="windows"):
        """Nothing of the fetch is left but the package and the folder unpacked from it."""
        self.assertEqual(sorted(os.listdir(self.folder(system))), sorted(["verified", NAMES[system]]))
        self.assertEqual(os.listdir(os.path.dirname(self.unpacked(system))), [TOP])

    def assert_nothing_kept(self, system="windows"):
        """No folder, no package, no staging."""
        self.assertLessEqual(set(os.listdir(self.folder(system))), {"verified"})
        self.assertFalse(os.path.exists(self.unpacked(system)))

    def refused(self, digests, system="windows"):
        with self.assertRaises(SystemExit) as caught:
            check_mod_abi.fetch(TAG, system, digests)
        return str(caught.exception.code)

    def test_pinned_package_is_unpacked_and_kept(self):
        for system in NAMES:
            with self.subTest(system=system):
                if system == "linux" and not hasattr(tarfile, "data_filter"):
                    self.skipTest("this Python's tarfile has no data filter (3.12, or 3.8.17+)")
                digests = self.publish(system)
                unpacked = check_mod_abi.fetch(TAG, system, digests)
                self.assertEqual(unpacked, self.unpacked(system))
                self.assertEqual(pathlib.Path(unpacked, "sdk", "exports.txt").read_bytes(), b"Mod_Name\n")
                kept = os.path.join(self.folder(system), NAMES[system])
                self.assertEqual(digest(kept), digests[(TAG, system)])
                # Nothing of the download is left but the folder and its package.
                self.assert_only_kept(system)

    def test_other_package_is_refused_before_unpacking(self):
        for system in NAMES:
            with self.subTest(system=system):
                self.publish(system)
                message = self.refused({(TAG, system): "0" * 64}, system)
                self.assertIn("is not the", message)
                self.assertIn("0" * 64, message)
                self.assertIn("nothing of it was unpacked", message)
                self.assert_nothing_kept(system)

    def test_unpinned_release_is_refused(self):
        self.publish()
        message = self.refused({(TAG, "linux"): "0" * 64})
        self.assertIn(f"sha256 {TAG} windows", message)
        self.assertIn("gh api", message)
        self.assertEqual(self.downloads, [])

    def test_cached_folder_beside_its_package_is_used_without_download(self):
        digests = self.publish()
        check_mod_abi.fetch(TAG, "windows", digests)
        os.remove(os.path.join(self.server, TAG, NAMES["windows"]))   # a download now would fail
        self.downloads.clear()
        self.assertTrue(os.path.isdir(check_mod_abi.fetch(TAG, "windows", digests)))
        self.assertEqual(self.downloads, [])

    def test_cached_package_without_folder_is_unpacked_without_download(self):
        digests = self.publish()
        unpacked = check_mod_abi.fetch(TAG, "windows", digests)
        check_mod_abi.shutil.rmtree(unpacked)
        self.downloads.clear()
        self.assertTrue(os.path.isfile(os.path.join(check_mod_abi.fetch(TAG, "windows", digests), "sdk", "exports.txt")))
        self.assertEqual(self.downloads, [])

    def test_folder_without_package_is_fetched_again(self):
        digests = self.publish()
        stale = self.unpacked()
        os.makedirs(os.path.join(stale, "mods", "planted"))
        pathlib.Path(stale, "mods", "planted", "mod.json").write_text("{}")
        unpacked = check_mod_abi.fetch(TAG, "windows", digests)
        self.assertEqual(len(self.downloads), 1)
        self.assertFalse(os.path.exists(os.path.join(unpacked, "mods", "planted")))
        self.assertTrue(os.path.isfile(os.path.join(unpacked, "mods", "a", "mod.json")))
        self.assert_only_kept()

    def test_older_checks_folder_beside_the_package_is_not_used(self):
        # A check from before the pinning unpacks to <tag>/<system>/TOP, with no
        # hash check, even beside a verified package; that folder is never used.
        digests = self.publish()
        check_mod_abi.shutil.rmtree(check_mod_abi.fetch(TAG, "windows", digests))
        older = os.path.join(self.folder(), TOP)
        os.makedirs(os.path.join(older, "mods", "planted"))
        self.downloads.clear()
        unpacked = check_mod_abi.fetch(TAG, "windows", digests)
        self.assertEqual(unpacked, self.unpacked())
        self.assertFalse(os.path.exists(os.path.join(unpacked, "mods", "planted")))
        self.assertTrue(os.path.isfile(os.path.join(unpacked, "mods", "a", "mod.json")))
        self.assertEqual(self.downloads, [])

    def plant_before_move_in(self, digests, with_package):
        """fetch(), with a folder put in its place just before it moves its own
        in: by an older check (no package) or by another run (its package)."""
        real, unpacked, planted = os.rename, self.unpacked(), []

        def rename(source, target):
            if target == unpacked and not planted:
                planted.append(target)
                os.makedirs(os.path.join(unpacked, "mods", "planted"))
                if with_package:
                    check_mod_abi.shutil.copy(os.path.join(self.server, TAG, NAMES["windows"]), self.folder())
            return real(source, target)
        with mock.patch.object(check_mod_abi.os, "rename", rename):
            return check_mod_abi.fetch(TAG, "windows", digests)

    def test_folder_put_in_meanwhile_without_package_is_not_trusted(self):
        digests = self.publish()
        unpacked = self.plant_before_move_in(digests, with_package=False)
        self.assertFalse(os.path.exists(os.path.join(unpacked, "mods", "planted")))
        self.assertTrue(os.path.isfile(os.path.join(unpacked, "mods", "a", "mod.json")))
        self.assert_only_kept()

    def test_folder_put_in_meanwhile_beside_its_package_is_used(self):
        digests = self.publish()
        unpacked = self.plant_before_move_in(digests, with_package=True)
        self.assertTrue(os.path.isdir(os.path.join(unpacked, "mods", "planted")))   # the other run's
        self.assert_only_kept()

    def test_changed_package_beside_folder_is_refused(self):
        digests = self.publish()
        check_mod_abi.fetch(TAG, "windows", digests)
        kept = os.path.join(self.folder(), NAMES["windows"])
        with open(kept, "ab") as handle:
            handle.write(b"\0")
        self.downloads.clear()
        message = self.refused(digests)
        self.assertIn(kept, message)
        self.assertIn("delete", message)
        self.assertEqual(self.downloads, [])

    def test_member_outside_the_folder_is_refused(self):
        for system in NAMES:
            for outside in ("../escape.txt", f"{TOP}/../escape.txt", "/absolute.txt", "other/file.txt",
                            "C:/drive.txt", f"{TOP}\\..\\escape.txt"):
                with self.subTest(system=system, member=outside):
                    digests = self.publish(system, {f"{TOP}/sdk/exports.txt": b"", outside: b"x"})
                    message = self.refused(digests, system)
                    self.assertIn("outside", message)
                    self.assert_nothing_kept(system)
                    self.assertFalse(os.path.exists(os.path.join(self.scratch.name, "escape.txt")))

    def test_package_without_its_folder_is_refused(self):
        digests = self.publish(members={f"{TOP}": b"a file, not the folder"})
        self.assertIn(f"no {TOP} folder", self.refused(digests))


class List(unittest.TestCase):
    def read(self, text):
        with tempfile.TemporaryDirectory() as scratch:
            path = os.path.join(scratch, "mod_compat.txt")
            pathlib.Path(path).write_text(text, encoding="utf-8")
            with mock.patch.object(check_mod_abi, "LIST", path):
                return check_mod_abi.read_list()

    def test_sha256_lines(self):
        baselines, accepted, digests = self.read(f"baseline {TAG}\nsha256 {TAG} windows {'a' * 64}\n"
                                                 f"sha256 {TAG} linux {'b' * 64}  # the tarball\n")
        self.assertEqual(baselines, [TAG])
        self.assertEqual(digests, {(TAG, "windows"): "a" * 64, (TAG, "linux"): "b" * 64})

    def test_malformed_sha256_lines(self):
        for line in (f"sha256 {TAG} windows {'a' * 63}", f"sha256 {TAG} windows {'A' * 64}",
                     f"sha256 {TAG} macos {'a' * 64}", f"sha256 {TAG} {'a' * 64}"):
            with self.subTest(line=line), self.assertRaises(SystemExit):
                self.read(line + "\n")

    def test_repository_list_pins_every_baseline(self):
        baselines, _, digests = check_mod_abi.read_list()
        for tag in baselines:
            for system in check_mod_abi.SYSTEMS:
                self.assertIn((tag, system), digests, f"mod_compat.txt: no `sha256 {tag} {system}`")


if __name__ == "__main__":
    unittest.main()
