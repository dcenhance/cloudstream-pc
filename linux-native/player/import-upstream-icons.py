#!/usr/bin/env python3
"""Convert the pinned CloudStream Android vectors to white Qt SVGs.
Source: recloudstream/cloudstream, GPL-3.0 (see repository LICENSE).
No geometry is redrawn. Original XML files and provenance are retained.
"""
from pathlib import Path
import urllib.request
import xml.etree.ElementTree as ET

REVISION = '81dbdf4b4483ee72566f108ac9cde998a79e519e'
NAMES = ['netflix_skip_forward', 'netflix_pause', 'netflix_play', 'video_locked',
         'ic_baseline_aspect_ratio_24', 'ic_baseline_speed_24', 'ic_baseline_equalizer_24',
         'ic_baseline_playlist_play_24', 'ic_baseline_arrow_back_24', 'baseline_fullscreen_24']
OUT = Path(__file__).parent / 'upstream'
A = '{http://schemas.android.com/apk/res/android}'
OUT.mkdir(exist_ok=True)
for name in NAMES:
    url = f'https://raw.githubusercontent.com/recloudstream/cloudstream/{REVISION}/app/src/main/res/drawable/{name}.xml'
    data = urllib.request.urlopen(url, timeout=30).read()
    (OUT / (name + '.xml')).write_bytes(data)
    root = ET.fromstring(data)
    svg = ET.Element('svg', {'xmlns': 'http://www.w3.org/2000/svg', 'viewBox': f'0 0 {root.get(A+"viewportWidth")} {root.get(A+"viewportHeight")}'})
    for path in root.findall('path'):
        attr = {'d': path.get(A+'pathData'), 'fill': 'none' if path.get(A+'fillColor') == '#00000000' else '#fff'}
        if path.get(A+'strokeColor'):
            attr.update({'stroke': '#fff', 'stroke-width': path.get(A+'strokeWidth', '1')})
        if path.get(A+'fillType') == 'evenOdd':
            attr['fill-rule'] = 'evenodd'
        ET.SubElement(svg, 'path', attr)
    (OUT / (name + '.svg')).write_text(ET.tostring(svg, encoding='unicode') + '\n')
print(f'Converted {len(NAMES)} vectors from {REVISION}')
