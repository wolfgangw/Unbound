#!/usr/bin/env python3
"""Compile the shared installer definition into a header for C++/Objective-C++."""
import json
import pathlib
import re
import sys


def generate(source):
    data = json.loads(source)
    if data['schema'] != 1:
        raise ValueError('Unsupported installer schema')
    width, height = data['size']
    if not all(type(v) is int and v > 0 for v in (width, height)):
        raise ValueError('Invalid dialog size')
    lines = ['// Generated from src/installer/dialog.json; do not edit.',
             '#pragma once', 'namespace InstallerUI {',
             'struct Rect { int x, y, width, height; };',
             f'inline constexpr int width={width}, height={height};']
    for group in ('fonts', 'rects', 'text'):
        lines.append(f'namespace {group} {{')
        for key, value in data[group].items():
            if not re.fullmatch(r'[a-zA-Z][a-zA-Z0-9]*', key):
                raise ValueError('Invalid identifier')
            if group == 'rects':
                x, y, w, h = value
                if not all(type(v) is int for v in value) or min(x, y) < 0 or min(w, h) <= 0 or x+w > width or y+h > height:
                    raise ValueError(f'Invalid rectangle: {key}')
                lines.append(f'inline constexpr Rect {key}={{{x},{y},{w},{h}}};')
            elif group == 'fonts':
                if type(value) is not int or value <= 0:
                    raise ValueError('Invalid font size')
                lines.append(f'inline constexpr int {key}={value};')
            else:
                if not isinstance(value, str) or not value.strip():
                    raise ValueError(f'Invalid text: {key}')
                lines.append(f'inline constexpr char {key}[]={json.dumps(value, ensure_ascii=True)};')
        lines.append('}')
    lines.append('}')
    return '\n'.join(lines)+'\n'


if __name__ == '__main__':
    source, destination = map(pathlib.Path, sys.argv[1:])
    result = generate(source.read_text(encoding='utf-8'))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('w', encoding='utf-8', newline='\n') as output:
        output.write(result)
