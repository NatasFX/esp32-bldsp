import importlib.util
from pathlib import Path
import unittest
import io,json
from unittest.mock import patch,MagicMock
spec=importlib.util.spec_from_file_location('wireless',Path(__file__).resolve().parents[1]/'tools/wireless.py');wireless=importlib.util.module_from_spec(spec);spec.loader.exec_module(wireless)
class WirelessTest(unittest.TestCase):
 def image(self,chip,project):
  b=bytearray(320);b[0]=0xe9;b[12]=chip;b[23]=1;b[32:36]=bytes.fromhex('3254cdab');b[80:80+len(project)]=project;return bytes(b)
 def test_s3(self):self.assertEqual(wireless.validate_image(self.image(9,b'bldsp_control'),'s3'),'00'*32)
 def test_c3(self):self.assertEqual(wireless.validate_image(self.image(5,b'bldsp_panel_c3'),'c3'),'00'*32)
 def test_cross_target(self):
  with self.assertRaises(RuntimeError):wireless.validate_image(self.image(5,b'bldsp_panel_c3'),'s3')
 def test_truncated(self):
  with self.assertRaises(RuntimeError):wireless.validate_image(b'bad','s3')
 def test_full_flash_not_app(self):
  with self.assertRaises(RuntimeError):wireless.validate_image(bytes(0x10000)+self.image(9,b'bldsp_control'),'s3')

 def test_confirm_both_targets(self):
  for target,chip,project,route in [('s3',9,b'bldsp_control','/api/s3/ota'),('c3',5,b'bldsp_panel_c3','/api/panel/ota')]:
   before=dict(boot=1,address=65536,supported=True,online=True,pending=False,busy=False)
   after=dict(boot=2,address=0x310000,sha256='00'*32,pending=False,online=True)
   def answer(req,timeout):
    if req.full_url.endswith('/begin'):d={'session':7}
    elif '/data?' in req.full_url:d={'offset':len(req.data)}
    elif req.full_url.endswith('/end'):d={}
    elif req.data is not None:d={}
    else:d=before if not getattr(answer,'seen',False) else after;answer.seen=True
    return io.StringIO(json.dumps(d))
   opener=MagicMock();opener.open.side_effect=answer
   with patch.object(wireless.Path,'read_bytes',return_value=self.image(chip,project)),patch.object(wireless.urllib.request,'build_opener',return_value=opener),patch.object(wireless.time,'sleep'):
    self.assertEqual(wireless.deploy(target,'test.bin')['boot'],2)
 def test_confirmation_requires_new_boot(self):
  status=dict(boot=1,address=65536,supported=True,pending=False,sha256='00'*32)
  opener=MagicMock();opener.open.side_effect=lambda *a,**k:io.StringIO(json.dumps(status))
  with patch.object(wireless.Path,'read_bytes',return_value=self.image(9,b'bldsp_control')),patch.object(wireless.urllib.request,'build_opener',return_value=opener),patch.object(wireless.time,'sleep'),patch.object(wireless.time,'monotonic',side_effect=[0,0,0,999]):
   with self.assertRaises(RuntimeError):wireless.deploy('s3','test.bin')

if __name__=='__main__':unittest.main()
