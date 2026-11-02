from pathlib import Path
w=Path('work');s=(w/'test_audio020.py').read_text().replace('020','022')
s=s.replace("('sound',True,True,True),","('sound',True,True,True),('sound-delay',True,True,True),('sound-legacy',True,True,True),")
s=s.replace("if name=='sound':","if name.startswith('sound'):")
s=s.replace("if clientAudio:env['SURFACE_PROBE_AUDIO']='1'","""if clientAudio:env['SURFACE_PROBE_AUDIO']='1'
 env.pop('SURFACE_PROBE_AUDIO_HOLD_MS',None);env.pop('SURFACE_PROBE_AUDIO_LEGACY',None)
 if name=='sound-delay':env['SURFACE_PROBE_AUDIO_HOLD_MS']='350'
 if name=='sound-legacy':env['SURFACE_PROBE_AUDIO_LEGACY']='1'""")
s=s.replace("assert 'CODEC name=avc420' in text","""assert 'CODEC name=avc420' in text
  packets=re.search(r'PROBE_AUDIO_PACKETS count=(\\d+) min_bytes=(\\d+) max_bytes=(\\d+)',output)
  assert packets and int(packets[1])>100 and packets[2]=='1920' and packets[3]=='1920',output[-2000:]
  metrics=[dict(token.split('=',1) for token in line.split()[2:]) for line in text.splitlines() if line.startswith('SurfaceDesk AUDIO_LATENCY ')]
  assert any(int(a['confirmed'])>0 for a in metrics),text[-2000:]
  if name=='sound-delay':assert any(float(a['client_hold_ms'])>=300 for a in metrics),metrics
  else:assert all(float(a['client_hold_ms'])<200 for a in metrics),metrics
  if name=='sound-legacy':assert 'PROBE_AUDIO_WAVE2' not in output
  else:
   wave=re.findall(r'PROBE_AUDIO_WAVE2 bytes=(\\d+) capture_age_ms=(-?\\d+) capture_timestamp=(\\d+)',output)
   assert wave and all(a[0]=='1920' and abs(int(a[1]))<500 and int(a[2])>65535 for a in wave),wave
  assert 'AUDIO_BUFFER requested_ms=40 actual_ms=' in text""")
(w/'test_audio022.py').write_text(s)
