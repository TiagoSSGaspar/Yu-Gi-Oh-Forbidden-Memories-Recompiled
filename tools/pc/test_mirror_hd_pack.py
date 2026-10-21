#!/usr/bin/env python3
"""mirror_hd_pack.py: which asset it takes and the check of the copy. No
network: the downloads are file:// URLs."""
import hashlib, os, pathlib, shutil, sys, tempfile, unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mirror_hd_pack as mirror

GOOD = "sha256:" + "a" * 64


def asset(name, url="file:///nowhere", digest=GOOD, size=0):
    return {"name": name, "browser_download_url": url, "digest": digest, "size": size}


class PickAsset(unittest.TestCase):
    def test_the_one_named_after_the_release(self):
        release = {"tag_name": "v0.3.0", "assets": [asset("yfm-redecomp-hd-mod-v0.2.0.zip"),
                                                    asset("yfm-redecomp-hd-mod-v0.3.0.zip")]}
        self.assertEqual(mirror.pick_asset(release)[0], "yfm-redecomp-hd-mod-v0.3.0.zip")

    def test_else_the_first_hd_zip(self):
        release = {"tag_name": "v0.3.0", "assets": [asset("yfm-redecomp-v0.3.0-windows.zip"),
                                                    asset("yfm-redecomp-hd-mod-v0.2.9.zip")]}
        self.assertEqual(mirror.pick_asset(release)[0], "yfm-redecomp-hd-mod-v0.2.9.zip")

    def test_no_hd_pack(self):
        with self.assertRaises(mirror.MirrorError):
            mirror.pick_asset({"tag_name": "v0.3.0", "assets": [asset("yfm-redecomp-v0.3.0-linux.tar.gz")]})

    def test_no_digest(self):
        with self.assertRaises(mirror.MirrorError):
            mirror.pick_asset({"tag_name": "v0.3.0", "assets": [asset("yfm-redecomp-hd-mod-v0.3.0.zip", digest=None)]})


class Mirror(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp()
        self.source = os.path.join(self.folder, "official.zip")
        with open(self.source, "wb") as stream:
            stream.write(b"PK\x05\x06" + bytes(18))
        self.sha = hashlib.sha256(open(self.source, "rb").read()).hexdigest()
        self.out = os.path.join(self.folder, "release")

    def tearDown(self):
        shutil.rmtree(self.folder)

    def release(self, digest=None, size=22):
        url = pathlib.Path(self.source).as_uri()
        return {"tag_name": "v0.3.0", "assets": [asset("yfm-redecomp-hd-mod-v0.3.0.zip", url,
                                                       digest or "sha256:" + self.sha, size)]}

    def test_copy_named_after_our_tag(self):
        path = mirror.mirror(self.release(), "v1.0.0", self.out)
        self.assertEqual(os.path.basename(path), "yfm-redecomp-hd-mod-v1.0.0.zip")
        self.assertEqual(open(path, "rb").read(), open(self.source, "rb").read())
        self.assertEqual(os.listdir(self.out), ["yfm-redecomp-hd-mod-v1.0.0.zip"])

    def test_wrong_sha256_leaves_nothing(self):
        with self.assertRaises(mirror.MirrorError):
            mirror.mirror(self.release(digest=GOOD), "v1.0.0", self.out)
        self.assertEqual(os.listdir(self.out), [])

    def test_wrong_size_leaves_nothing(self):
        with self.assertRaises(mirror.MirrorError):
            mirror.mirror(self.release(size=999), "v1.0.0", self.out)
        self.assertEqual(os.listdir(self.out), [])


if __name__ == "__main__":
    unittest.main()
