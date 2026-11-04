import json, subprocess, base64, os, sys, io, struct, hashlib

proc = subprocess.Popen(["./miime-mcp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(obj):
    proc.stdin.write(json.dumps(obj) + "\n")
    proc.stdin.flush()

def recv():
    line = proc.stdout.readline()
    return json.loads(line)

# initialize
send({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})
r = recv()
assert "result" in r, r
assert "tools" in r["result"]["capabilities"]

send({"jsonrpc":"2.0","method":"notifications/initialized"})
# tools/list
send({"jsonrpc":"2.0","id":2,"method":"tools/list"})
r = recv()
names = [t["name"] for t in r["result"]["tools"]]
assert "prepare_ux_canvas" in names
assert "render_ux_annotations" in names
assert "patch_ux_annotations" in names
assert "get_ux_annotations" in names
assert "verify_ux_render" in names
tools_by_name = {t["name"]: t for t in r["result"]["tools"]}
assert set(tools_by_name) == {"prepare_ux_canvas", "render_ux_annotations", "patch_ux_annotations", "get_ux_annotations", "verify_ux_render"}
for tool in tools_by_name.values():
    assert tool["inputSchema"]["type"] == "object", tool
render_schema = tools_by_name["render_ux_annotations"]["inputSchema"]
assert "freehand" in render_schema["properties"]["annotations"]["items"]["properties"]["type"]["enum"]
assert render_schema["properties"]["dry_run"] == {"type": "boolean"}
for config_field in ("drawing_style", "aesthetic_style", "theme", "analysis_background", "reset_render_config"):
    assert config_field in render_schema["properties"], config_field
assert "dry_run" not in render_schema.get("required", [])
annotation_schema = render_schema["properties"]["annotations"]["items"]["properties"]
assert len(annotation_schema["stroke_width"]["oneOf"]) == 2
assert annotation_schema["stroke_width"]["oneOf"][0]["exclusiveMinimum"] == 0
assert annotation_schema["stroke_width"]["oneOf"][1]["minItems"] == annotation_schema["stroke_width"]["oneOf"][1]["maxItems"] == 2
assert "source" in annotation_schema and "font_size" in annotation_schema and "label" in annotation_schema
assert "som_id" in annotation_schema and "ink" in annotation_schema and "fill" in annotation_schema
assert annotation_schema["label"]["properties"]["anchor"]["oneOf"][1]["maximum"] == 7
assert annotation_schema["label"]["properties"]["anchor"]["oneOf"][0]["enum"][-8:] == [str(i) for i in range(8)]
assert render_schema == tools_by_name["patch_ux_annotations"]["inputSchema"]
assert "annotations" in tools_by_name["patch_ux_annotations"]["inputSchema"]["properties"]
assert tools_by_name["get_ux_annotations"]["inputSchema"] == {"type": "object", "properties": {}}
assert tools_by_name["verify_ux_render"]["inputSchema"] == {"type": "object", "properties": {}}
prepare_schema = tools_by_name["prepare_ux_canvas"]["inputSchema"]
assert "source" in prepare_schema["required"] and "viewport" in prepare_schema["properties"]

# malformed input
send({"jsonrpc":"2.0","id":3,"method":"foo/bar"})
r = recv()
assert "error" in r, r

# prepare
img = "/tmp/opencode/mcp_test_in.png"
import cairo
s = cairo.ImageSurface(cairo.FORMAT_ARGB32, 64, 64)
cr = cairo.Context(s)
cr.set_source_rgb(1,1,1); cr.paint()
s.write_to_png(img)

send({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"prepare_ux_canvas","arguments":{"source_type":"image_path","source":img,"visual_aids":True}}})
r = recv()
assert "result" in r, r
assert any(c.get("type")=="image" for c in r["result"]["content"])

# The headless JSON route and MCP must share the same automatic-theme layout
# and export pixels for the same source and requested config.
headless_case = {"drawing_style":"editorial", "theme":"auto", "annotations":[
    {"id":"x", "type":"rect", "points":[10,10,50,50], "text":""}]}
send({"jsonrpc":"2.0","id":65,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":headless_case}})
headless_mcp = recv()
assert "result" in headless_mcp, headless_mcp
mcp_png = base64.b64decode(next(c["data"] for c in headless_mcp["result"]["content"] if c.get("type")=="image"))
with open("/tmp/opencode/test_headless_out.png", "rb") as headless_file:
    headless_png = headless_file.read()
assert hashlib.sha256(mcp_png).digest() == hashlib.sha256(headless_png).digest()
cli_annotations = "/tmp/opencode/test_headless_cli.json"
cli_output = "/tmp/opencode/test_headless_cli.png"
with open(cli_annotations, "w", encoding="utf-8") as cli_file:
    json.dump(headless_case, cli_file)
subprocess.run(["./miime", "--headless", img, "--annotations", cli_annotations, "-o", cli_output], check=True)
with open(cli_output, "rb") as cli_file:
    assert hashlib.sha256(mcp_png).digest() == hashlib.sha256(cli_file.read()).digest()

# Explicit style config opts into the resolver; a legacy alias by itself keeps
# the 2.1 path, while alias mapping and conflict diagnostics apply under opt-in.
def dry_render(args):
    call = {"jsonrpc":"2.0", "id": 57, "method":"tools/call",
            "params":{"name":"render_ux_annotations", "arguments":dict(args, dry_run=True)}}
    send(call)
    response = recv()
    assert "result" in response, response
    return json.loads(next(c["text"] for c in response["result"]["content"] if c.get("type")=="text"))

alias_only = dry_render({"aesthetic_style":"clean_technical", "annotations":[{"type":"rect","points":[10,10,30,30]}]})
assert "resolution" not in alias_only, alias_only
alias_optin = dry_render({"theme":"dark", "aesthetic_style":"clean_technical", "annotations":[{"type":"rect","points":[10,10,30,30]}]})
assert alias_optin["resolution"]["drawing_style"] == "technical", alias_optin
conflict = dry_render({"drawing_style":"editorial", "theme":"dark", "aesthetic_style":"clean_technical", "annotations":[{"type":"rect","points":[10,10,30,30]}]})
assert conflict["resolution"]["drawing_style"] == "editorial", conflict
assert "style_alias_conflict" in conflict["resolution"]["warning_codes"], conflict

# Patch config is requested state: omitted fields inherit, reset clears before
# fields in the same request apply, and the reset command itself is not saved.
send({"jsonrpc":"2.0","id":58,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"theme":"dark","drawing_style":"editorial","annotations":[{"type":"rect","points":[10,10,30,30]}],"dry_run":True}}})
assert "result" in recv()
send({"jsonrpc":"2.0","id":59,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"analysis_background":"#aabbcc","annotations":[{"type":"rect","points":[10,10,30,30]}],"dry_run":True}}})
inherited_patch = recv()
assert "result" in inherited_patch, inherited_patch
inherited_info = json.loads(next(c["text"] for c in inherited_patch["result"]["content"] if c.get("type")=="text"))
assert inherited_info["resolution"]["theme"] == 2 and inherited_info["resolution"]["drawing_style"] == "editorial", inherited_info
send({"jsonrpc":"2.0","id":60,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
stored = json.loads(recv()["result"]["content"][0]["text"])
assert stored["theme"] == "dark" and stored["drawing_style"] == "editorial" and stored["analysis_background"] == "#AABBCC", stored
send({"jsonrpc":"2.0","id":61,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"reset_render_config":True,"theme":"warm","annotations":[{"type":"rect","points":[10,10,30,30]}],"dry_run":True}}})
assert "result" in recv()
send({"jsonrpc":"2.0","id":62,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
stored = json.loads(recv()["result"]["content"][0]["text"])
assert stored["theme"] == "warm" and "drawing_style" not in stored and "analysis_background" not in stored and "reset_render_config" not in stored, stored

# Invalid config fails before annotation/config state changes.
before_invalid = json.dumps(stored, sort_keys=True)
send({"jsonrpc":"2.0","id":63,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"theme":"ultraviolet","annotations":[{"type":"rect","points":[1,1,3,3]}]}}})
assert "error" in recv()
send({"jsonrpc":"2.0","id":64,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
assert json.dumps(json.loads(recv()["result"]["content"][0]["text"]), sort_keys=True) == before_invalid

# Two consecutive request-only themes resolve independently.
request_light = dry_render({"theme":"light", "annotations":[{"type":"rect","points":[10,10,30,30]}]})
request_dark = dry_render({"theme":"dark", "annotations":[{"type":"rect","points":[10,10,30,30]}]})
assert request_light["resolution"]["theme"] != request_dark["resolution"]["theme"]
legacy_after_scoped = dry_render({"annotations":[{"type":"rect","points":[10,10,30,30]}]})
assert "resolution" not in legacy_after_scoped, legacy_after_scoped

# No-label MCP exports must honor z ordering and marker's forced-under layer.
def render_pixel(annotations, x=32, y=32):
    send({"jsonrpc":"2.0","id":56,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"aesthetic_style":"clean_technical","annotations":annotations}}})
    response=recv()
    assert "result" in response, response
    image=next(c["data"] for c in response["result"]["content"] if c.get("type")=="image")
    surface=cairo.ImageSurface.create_from_png(io.BytesIO(base64.b64decode(image)))
    surface.flush()
    return struct.unpack_from("=I",surface.get_data(),y*surface.get_stride()+x*4)[0]

under_pixel=render_pixel([
    {"type":"callout","points":[10,10,54,54],"color":[1,0,0,1],"fill":"solid","fill_color":[1,0,0,1],"stroke_width":1},
    {"type":"callout","points":[10,10,54,54],"color":[0,0,1,1],"fill":"solid","fill_color":[0,0,1,1],"stroke_width":1,"z":"under"},
])
assert ((under_pixel>>16)&255)>245 and (under_pixel&255)<10, hex(under_pixel)
over_pixel=render_pixel([
    {"type":"callout","points":[10,10,54,54],"color":[1,0,0,1],"fill":"solid","fill_color":[1,0,0,1],"stroke_width":1,"z":"over"},
    {"type":"callout","points":[10,10,54,54],"color":[0,0,1,1],"fill":"solid","fill_color":[0,0,1,1],"stroke_width":1},
])
assert ((over_pixel>>16)&255)>245 and (over_pixel&255)<10, hex(over_pixel)
marker_pixel=render_pixel([
    {"type":"callout","points":[10,10,54,54],"color":[1,0,0,1],"fill":"solid","fill_color":[1,0,0,1],"stroke_width":1},
    {"type":"line","points":[10,32,54,32],"color":[0,1,0,1],"stroke_width":12,"ink":"marker"},
])
assert ((marker_pixel>>16)&255)>245 and ((marker_pixel>>8)&255)<10, hex(marker_pixel)
registry_text = next(c["text"] for c in r["result"]["content"] if c.get("type")=="text")
registry = json.loads(registry_text)
known_ids = {item["id"] for item in registry}

# `extract_int` searches the whole arguments JSON, so the nested viewport form
# advertised by the schema reaches grounding and scales its prepared buffer.
send({"jsonrpc":"2.0","id":41,"method":"tools/call","params":{"name":"prepare_ux_canvas","arguments":{"source_type":"image_path","source":img,"visual_aids":False,"viewport":{"width":32,"height":24}}}})
r = recv()
nested_viewport_image = next(c for c in r["result"]["content"] if c.get("type")=="image")
nested_viewport_surface = cairo.ImageSurface.create_from_png(io.BytesIO(base64.b64decode(nested_viewport_image["data"])))
assert (nested_viewport_surface.get_width(), nested_viewport_surface.get_height()) == (32, 24)
send({"jsonrpc":"2.0","id":42,"method":"tools/call","params":{"name":"prepare_ux_canvas","arguments":{"source_type":"image_path","source":img,"visual_aids":True}}})
recv()

# Margin rail expands the exported image and offsets source content into the
# center 300px gutter on each side. Dry-run returns placements only.
rail_annotations = [
    {"id":"left-a","type":"rect","points":[5,5,15,15],"label":{"text":"Left α","anchor":"right","leader":"elbow"}},
    {"id":"right-b","type":"rect","points":[45,5,55,15],"label":{"text":"Right β","leader":"curved"}},
    {"id":"left-c","type":"rect","points":[8,35,18,45],"label":{"text":"Lower γ","leader":"none"}},
]
send({"jsonrpc":"2.0","id":50,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"layout_mode":"margin_rail","aesthetic_style":"clean_technical","annotations":rail_annotations}}})
r = recv()
assert "result" in r, r
rail_image = next(c for c in r["result"]["content"] if c.get("type")=="image")
rail_surface = cairo.ImageSurface.create_from_png(io.BytesIO(base64.b64decode(rail_image["data"])))
assert rail_surface.get_width() == 664 and rail_surface.get_height() == 64
rail_surface.flush()
rail_bytes = rail_surface.get_data()
assert struct.unpack_from("=I",rail_bytes,60*rail_surface.get_stride()+5*4)[0] == 0
assert struct.unpack_from("=I",rail_bytes,60*rail_surface.get_stride()+305*4)[0] != 0

send({"jsonrpc":"2.0","id":51,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"layout_mode":"margin_rail","dry_run":True,"annotations":rail_annotations}}})
r = recv()
assert "result" in r, r
assert not any(c.get("type")=="image" for c in r["result"]["content"]), r
placement_payload = json.loads(next(c["text"] for c in r["result"]["content"] if c.get("type")=="text"))
assert len(placement_payload["placements"]) == 3
assert isinstance(placement_payload["placements"][0]["label_x"], (int,float))
assert isinstance(placement_payload["placements"][0]["label_y"], (int,float))
assert placement_payload["placements"][0]["label_x"] < 300
assert placement_payload["placements"][1]["label_x"] > 300
send({"jsonrpc":"2.0","id":40,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"annotations":[{"type":"highlight_box","som_id":0}]}}})
r = recv()
assert ("result" in r) == (0 in known_ids), r
if 0 not in known_ids:
    assert r["error"]["message"] == "unknown som_id 0 (or missing bounds)", r

# render
send({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"annotations":[{"id":"a","type":"highlight_box","points":[10,10,50,50],"color":[1,0,0,1],"stroke_width":2,"text":"","font_size":18}]}}})
r = recv()
assert "result" in r, r
assert any(c.get("type")=="image" for c in r["result"]["content"])
first_render = next(c["data"] for c in r["result"]["content"] if c.get("type")=="image")
send({"jsonrpc":"2.0","id":55,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"annotations":[{"id":"a","type":"highlight_box","points":[10,10,50,50],"color":[1,0,0,1],"stroke_width":2,"text":"","font_size":18}]}}})
r = recv()
second_render = next(c["data"] for c in r["result"]["content"] if c.get("type")=="image")
assert hashlib.sha256(base64.b64decode(first_render)).digest() == hashlib.sha256(base64.b64decode(second_render)).digest()

# get
send({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
r = recv()
assert "result" in r

# patch
send({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"annotations":[{"id":"a","type":"rect","points":[5,5,20,20],"color":[0,1,0,1],"stroke_width":2,"text":"","font_size":18}]}}})
r = recv()
assert "result" in r

# Patch forwards layout and dry-run arguments to the renderer.
send({"jsonrpc":"2.0","id":73,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"layout_mode":"margin_rail","dry_run":True,"annotations":[{"id":"patch-label","type":"rect","points":[20,20,40,40],"label":{"text":"patched"}}]}}})
r = recv()
assert "result" in r and not any(c.get("type")=="image" for c in r["result"]["content"]), r
assert "label_x" in next(c["text"] for c in r["result"]["content"] if c.get("type")=="text")

# A failed targeted patch reports a JSON-RPC error and preserves annotations.
send({"jsonrpc":"2.0","id":70,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
before_failed_patch = recv()["result"]["content"][0]["text"]
send({"jsonrpc":"2.0","id":69,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"annotations":[{"type":"highlight_box","som_id":-2147483648}]}}})
r = recv()
assert "error" in r and "unknown som_id -2147483648" in r["error"]["message"], r
send({"jsonrpc":"2.0","id":68,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
assert recv()["result"]["content"][0]["text"] == before_failed_patch
send({"jsonrpc":"2.0","id":71,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"annotations":[{"type":"highlight_box","som_id":-2147483648}]}}})
r = recv()
assert "error" in r and "unknown som_id -2147483648" in r["error"]["message"], r
send({"jsonrpc":"2.0","id":72,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
after_failed_patch = recv()["result"]["content"][0]["text"]
assert after_failed_patch == before_failed_patch, (before_failed_patch, after_failed_patch)

# Invalid widths are real schema errors and cannot be hidden by the previous
# valid state or partially applied by patch.
send({"jsonrpc":"2.0","id":74,"method":"tools/call","params":{"name":"render_ux_annotations","arguments":{"annotations":[{"type":"rect","stroke_width":[2,"wide"]}]}}})
r = recv()
assert "error" in r and "stroke_width" in r["error"]["message"], r
send({"jsonrpc":"2.0","id":75,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
before_bad_width = recv()["result"]["content"][0]["text"]
send({"jsonrpc":"2.0","id":76,"method":"tools/call","params":{"name":"patch_ux_annotations","arguments":{"annotations":[{"type":"rect","stroke_width":[2,"wide"]}]}}})
r = recv()
assert "error" in r and "stroke_width" in r["error"]["message"], r
send({"jsonrpc":"2.0","id":77,"method":"tools/call","params":{"name":"get_ux_annotations","arguments":{}}})
assert recv()["result"]["content"][0]["text"] == before_bad_width

# verify
send({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"verify_ux_render","arguments":{}}})
r = recv()
assert "result" in r

proc.stdin.close()
proc.wait(timeout=5)

# Legacy pixel compatibility guard. Exact Cairo/font output can vary by platform,
# so these hashes are a local regression guard for the project CI renderer.
golden_proc = subprocess.Popen(["./miime-mcp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
def golden_call(name, args):
    golden_proc.stdin.write(json.dumps({"jsonrpc":"2.0","id":90,"method":"tools/call","params":{"name":name,"arguments":args}}) + "\n")
    golden_proc.stdin.flush()
    return json.loads(golden_proc.stdout.readline())
fixture_dir = os.path.join(os.path.dirname(__file__), "fixtures")
golden_call("prepare_ux_canvas", {"source_type":"image_path","source":os.path.join(fixture_dir,"legacy-compat-base.png"),"visual_aids":False})
with open(os.path.join(fixture_dir,"legacy-render-goldens.json"), encoding="utf-8") as f:
    for case in json.load(f):
        rendered = golden_call("render_ux_annotations", case["args"])
        png = next(c["data"] for c in rendered["result"]["content"] if c.get("type")=="image")
        assert hashlib.sha256(base64.b64decode(png)).hexdigest() == case["sha256"], case["args"]["annotations"][0]["type"]
golden_proc.stdin.close()
golden_proc.wait(timeout=5)

# Run all primitive and layout PNG cases in two fresh MCP processes. This catches
# hidden process-level randomness in addition to the repeated-call check above.
def fresh_png_hashes():
    p = subprocess.Popen(["./miime-mcp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    def call(name, args):
        p.stdin.write(json.dumps({"jsonrpc":"2.0","id":91,"method":"tools/call","params":{"name":name,"arguments":args}}) + "\n")
        p.stdin.flush()
        return json.loads(p.stdout.readline())
    call("prepare_ux_canvas", {"source_type":"image_path","source":os.path.join(fixture_dir,"legacy-compat-base.png"),"visual_aids":False})
    hashes = []
    primitive_types = render_schema["properties"]["annotations"]["items"]["properties"]["type"]["enum"]
    for primitive in primitive_types:
        case = {"annotations":[{"type":primitive,"points":[20,20,100,100],"text":"","font_size":16}]}
        result = call("render_ux_annotations", case)
        image = next(c["data"] for c in result["result"]["content"] if c.get("type")=="image")
        hashes.append(hashlib.sha256(base64.b64decode(image)).hexdigest())
    layout_case = {"layout_mode":"margin_rail","aesthetic_style":"clean_technical","annotations":[{"type":"rect","points":[20,20,100,100],"label":{"text":"A layout label","anchor":"right","leader":"elbow"}}]}
    result = call("render_ux_annotations", layout_case)
    image = next(c["data"] for c in result["result"]["content"] if c.get("type")=="image")
    hashes.append(hashlib.sha256(base64.b64decode(image)).hexdigest())
    p.stdin.close()
    p.wait(timeout=5)
    return hashes

assert fresh_png_hashes() == fresh_png_hashes(), "PNG output changed between fresh MCP processes"
print("MCP integration test passed")
