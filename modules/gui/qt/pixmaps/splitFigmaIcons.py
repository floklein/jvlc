#! /usr/bin/env python3
# -*- coding: utf-8 -*-
import copy
import os
import re
import sys
import xml.etree.ElementTree as ET

import pathops
from picosvg.svg import SVG
from picosvg.svg_pathops import skia_path, svg_commands
from picosvg.svg_types import SVGPath

DOC = """
turn the SVG export of the redesign's icon component set into the glyph SVGs
used by makeIconFont.py

The input is the whole "Icon" component set (Figma node 42:356) exported as
one SVG. Every variant is a 24x24 frame on a 44 px grid, drawn with 1.5 px
strokes. For each variant this script:

  - outlines the strokes and applies the transforms (picosvg)
  - flattens the shapes in paint order: a shape knocks out what is below it
    in the other color layers, so painted cut-outs become real holes
  - writes one fill-only 24x24 SVG per color layer

Glyph keys come from the variant properties:

  name[_direction][_value][_eq preset][_state][_tt]

with "_tt" for two-tone variants. A variant drawn in one color gives one
glyph, <key>.svg. A variant drawn with the base gray and an accent color
gives two glyphs to stack, <key>_base.svg and <key>_accent.svg. Two-tone
variants are always split, so they give <key>_tt_base.svg (if any gray) and
<key>_tt_accent.svg.

needs picosvg and skia-pathops (pip install picosvg skia-pathops)
"""

SVGNS = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVGNS)

# Figma paint styles of the Dark theme the icon set is bound to
LAYER_OF_COLOR = {
    "#8f8f8f": "base",        # FG/A2
    "#373737": "base",        # divider
    "#d37918": "accent",      # Accent/Main
    "#ab6314": "accent",      # Accent/Dim
    "#303030": "background",  # painted cut-outs
    "#292929": "background",
}
ACCENT_DIM = "#ab6314"

# variants that are not icons
SKIPPED_NAMES = {"divider", "thumbnail", "0"}

# fixes for the component set's naming errors
RENAMED = {
    ("close", "medium"): "volume",
    ("close", "low"): "volume",
    ("close", "none"): "volume",
    ("spectometer", "default"): "spectrometer",
    # "bookmarks" is the plain ribbon, "bookmark" has a plus on it
    ("bookmark", "default"): "add bookmark",
    # the icon of the Playlists section; the Queue button uses "lists"
    ("playlist", "default"): "playlists",
}
# directions that are the only one of their icon
DROPPED_DIRECTIONS = {"repeat", "shuffle", "upload"}


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def variant_key(props):
    name = props["name"]
    name = RENAMED.get((name, props["value"]), name)
    parts = [slug(name)]
    if props["direction"] != "default" and name not in DROPPED_DIRECTIONS:
        parts.append(props["direction"])
    if props["value"] != "default":
        parts.append(props["value"])
    if props["EQ"] != "default":
        parts.append(slug(props["EQ"]))
    if props["state"] != "default":
        parts.append(props["state"])
    if props["two-tone"] == "yes":
        parts.append("tt")
    return "_".join(parts)


def to_skia(shape):
    return skia_path(shape.as_cmd_seq(), getattr(shape, "fill_rule", "nonzero"))


def to_d(path):
    return SVGPath.from_commands(svg_commands(path)).round_floats(3).d


def flatten(group):
    """return (x, y, colors, {layer: path}) for one variant group"""
    doc = ET.Element(f"{{{SVGNS}}}svg",
                     {"width": "548", "height": "812", "viewBox": "0 0 548 812", "fill": "none"})
    doc.append(copy.deepcopy(group))
    shapes = list(SVG.fromstring(ET.tostring(doc, encoding="unicode")).topicosvg().shapes())
    if not shapes:
        return None

    everything = pathops.Path()
    for shape in shapes:
        everything.addPath(to_skia(shape))
    xmin, ymin, xmax, ymax = everything.bounds
    x = 20 + 44 * round(((xmin + xmax) / 2 - 32) / 44)
    y = 20 + 44 * round(((ymin + ymax) / 2 - 32) / 44)

    layers = {"base": pathops.Path(), "accent": pathops.Path(), "background": pathops.Path()}
    colors = set()
    for shape in shapes:
        color = shape.fill.lower()
        layer = LAYER_OF_COLOR[color]
        colors.add(color)
        path = to_skia(shape)
        for other in layers:
            if other != layer:
                layers[other] = pathops.op(layers[other], path, pathops.PathOp.DIFFERENCE)
        layers[layer] = pathops.op(layers[layer], path, pathops.PathOp.UNION)

    out = {}
    for layer in ("base", "accent"):
        if list(layers[layer].segments):
            out[layer] = layers[layer].transform(1, 0, 0, 1, -x, -y)
    return x, y, colors, out


def write_glyph(outdir, key, path):
    with open(os.path.join(outdir, key + ".svg"), "w") as fd:
        fd.write('<svg width="24" height="24" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">\n')
        fd.write(f'<path d="{to_d(path)}" fill="#000000"/>\n')
        fd.write('</svg>\n')


def main(src, outdir):
    root = ET.parse(src).getroot()
    iconset = root.find(f"{{{SVGNS}}}g")
    os.makedirs(outdir, exist_ok=True)

    variants = []
    for group in iconset.findall(f"{{{SVGNS}}}g"):
        # Figma suffixes duplicate layer names with _2, _3...
        name = re.sub(r"_\d+$", "", group.get("id"))
        props = dict(p.split("=", 1) for p in name.split(", "))
        if props["name"] in SKIPPED_NAMES:
            continue
        flat = flatten(group)
        if flat is None:
            continue
        variants.append((props, flat))

    keys = {}
    for props, (x, y, colors, layers) in variants:
        # two-tone variants are recognizable by their dim accent; some are
        # mislabeled two-tone=no
        if ACCENT_DIM in colors and props["two-tone"] == "no":
            twin = dict(props, **{"two-tone": "yes"})
            if not any(p == twin for p, _ in variants):
                props = twin
            else:
                # a stray dim detail on a monochrome icon
                merged = pathops.op(layers["base"], layers["accent"], pathops.PathOp.UNION)
                layers = {"base": merged}

        key = variant_key(props)
        if key in keys:
            raise ValueError(f"duplicate key {key} at {x},{y} and {keys[key]}")
        keys[key] = (x, y)

        if props["two-tone"] == "yes" or len(layers) > 1:
            for layer, path in layers.items():
                write_glyph(outdir, f"{key}_{layer}", path)
                print(f"{key}_{layer}")
        else:
            (path,) = layers.values()
            write_glyph(outdir, key, path)
            print(key)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=DOC, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("iconset", help="SVG export of the Figma icon component set")
    parser.add_argument("outdir", help="output directory of the glyph SVGs")
    args = parser.parse_args()
    main(args.iconset, args.outdir)
