from fastapi import FastAPI, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from OCC.Core.StlAPI import StlAPI_Reader
from OCC.Core.TopoDS import TopoDS_Compound
from OCC.Core.BRep import BRep_Builder 
from OCC.Core.BRepMesh import BRepMesh_IncrementalMesh
from OCC.Extend.TopologyUtils import TopologyExplorer
import tempfile
import os

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.post("/api/convert-step")
async def upload_mesh(file: UploadFile = File(...)):
    suffix = os.path.splitext(file.filename)[-1].lower()
    print(f"📦 Received file: {file.filename}  |  Extension: {suffix}")

    with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as tmp:
        tmp.write(await file.read())
        tmp_path = tmp.name

    try:
        if suffix in [".step", ".stp"]:
            from OCC.Extend.DataExchange import read_step_file
            shape = read_step_file(tmp_path)

        elif suffix == ".stl":
            reader = StlAPI_Reader()
            builder = BRep_Builder()
            compound = TopoDS_Compound()
            builder.MakeCompound(compound)

            if not reader.ReadFile(tmp_path):
                raise RuntimeError("Failed to read STL file")

            reader.Read(compound, builder)
            shape = compound

        elif suffix == ".obj":
            raise NotImplementedError("OBJ format support is not yet implemented")

        else:
            raise ValueError(f"Unsupported file format: {suffix}")

        # Mesh the shape and count faces
        BRepMesh_IncrementalMesh(shape, 0.1)
        exp = TopologyExplorer(shape)
        triangle_count = sum(1 for _ in exp.faces())

        return JSONResponse({"status": "ok", "faces": triangle_count})

    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=500)

    finally:
        os.remove(tmp_path)
