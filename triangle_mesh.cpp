// Source file for fracplanet
// Copyright (C) 2002,2003 Tim Day
/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "triangle_mesh.h"
#include <fstream>

void TriangleMesh::progress_start(uint steps,const std::string& info) const
{
  if (_progress) _progress->progress_start(steps,info);
}
void TriangleMesh::progress_step(uint step) const
{
  if (_progress) _progress->progress_step(step);
}
void TriangleMesh::progress_complete(const std::string& info) const
{
  if (_progress) _progress->progress_complete(info);
}

const XYZ TriangleMesh::triangle_normal(uint i) const
{
  const Triangle& t=triangle(i);
  const XYZ& v0=vertex(t.vertex(0)).position();
  const XYZ& v1=vertex(t.vertex(1)).position();
  const XYZ& v2=vertex(t.vertex(2)).position();

  const XYZ n((v1-v0)*(v2-v0));

  return n.normalised();
}

void TriangleMesh::compute_vertex_normals()
{
  const uint steps=triangles()+vertices();
  uint step=0;

  progress_start(100,"Compute normals");

  {
    std::vector<std::vector<uint> > vertices_to_triangles(vertices());
    for (uint i=0;i<triangles();i++)
      {
	step++;
	progress_step((100*step)/steps);      
      
	const Triangle& t=triangle(i);
	vertices_to_triangles[t.vertex(0)].push_back(i);
	vertices_to_triangles[t.vertex(1)].push_back(i);
	vertices_to_triangles[t.vertex(2)].push_back(i);
      }
  
    for (uint i=0;i<vertices();i++)
      {
	step++;
	progress_step((100*step)/steps);

	XYZ n(0.0,0.0,0.0);
	for (uint j=0;j<vertices_to_triangles[i].size();j++)
	  {
	    n+=triangle_normal(vertices_to_triangles[i][j]);
	  }
	n/=vertices_to_triangles[i].size();

	vertex(i).normal(n);
      }
  }
  
  progress_complete("Normals computed");
}

void TriangleMesh::subdivide(const XYZ& variation)
{
  const uint steps=vertices()+triangles();
  uint step=0;

  progress_start(100,"Subdividing");
  
  {
    // Make a copy of our data
    //! \todo Could be done more efficiently
    std::vector<Vertex> old_vertex(_vertex);
    std::vector<Triangle> old_triangle(_triangle); 

    // Clear ourself out
    _vertex.clear();
    _triangle.clear();

    // Copy back all vertices and perturb
    for (uint v=0;v<old_vertex.size();v++)
      {
	step++;
	progress_step((100*step)/steps);

	_vertex.push_back(Vertex(geometry().perturb(old_vertex[v].position(),variation)));
      }
  
    // Build a map from edges to new midpoints
    typedef std::map<TriangleEdge,uint> EdgeMap;
    EdgeMap edge_map;

    // Create new vertices and new triangles
    for (uint t=0;t<old_triangle.size();t++)
      {
	step++;
	progress_step((100*step)/steps);

	// These are the existing vertices
	const uint i0=old_triangle[t].vertex(0);
	const uint i1=old_triangle[t].vertex(1);
	const uint i2=old_triangle[t].vertex(2);
      
	// These are the edges
	TriangleEdge e01(i0,i1);
	TriangleEdge e12(i1,i2);
	TriangleEdge e20(i2,i0);

	// Find each edge in the map
	// If any edges don't exist, create their new mid-point and remember it
	EdgeMap::const_iterator e01it=edge_map.find(e01);
	EdgeMap::const_iterator e12it=edge_map.find(e12);
	EdgeMap::const_iterator e20it=edge_map.find(e20);

	const bool e01needed=(e01it==edge_map.end());
	const bool e12needed=(e12it==edge_map.end());
	const bool e20needed=(e20it==edge_map.end());

	if (e01needed)
	  {
	    e01it=edge_map.insert(EdgeMap::value_type(e01,_vertex.size())).first;
	    _vertex.push_back(Vertex(geometry().perturb(geometry().midpoint(vertex(i0).position(),vertex(i1).position()),variation)));
	  }

	if (e12needed)
	  {
	    e12it=edge_map.insert(EdgeMap::value_type(e12,_vertex.size())).first;
	    _vertex.push_back(Vertex(geometry().perturb(geometry().midpoint(vertex(i1).position(),vertex(i2).position()),variation)));
	  }

	if (e20needed)
	  {
	    e20it=edge_map.insert(EdgeMap::value_type(e20,_vertex.size())).first;
	    _vertex.push_back(Vertex(geometry().perturb(geometry().midpoint(vertex(i2).position(),vertex(i0).position()),variation)));
	  }
      
	// Create the subdivided triangles

	_triangle.push_back(Triangle(i0,(*e01it).second,(*e20it).second));
	_triangle.push_back(Triangle((*e01it).second,i1,(*e12it).second));
	_triangle.push_back(Triangle((*e20it).second,(*e12it).second,i2));
	_triangle.push_back(Triangle((*e01it).second,(*e12it).second,(*e20it).second));
      }
  }
  
  progress_complete("Subdivision completed");
}

void TriangleMesh::subdivide(uint subdivisions,uint flat_subdivisions,const XYZ& variation)
{
  for (uint s=0;s<subdivisions;s++)
    {
      if (s<flat_subdivisions)
	subdivide(XYZ(0.0,0.0,0.0));
      else
	subdivide(variation/(1<<s));
    }
}

void TriangleMesh::write_povray(const std::string& fname_base,const std::string& header,bool exclude_alternate_colour) const
{
  // \todo: No need to dump all vertices when not outputing all triangles.

  const uint triangles_to_output=(exclude_alternate_colour ? triangles_of_colour0() : triangles());

  // The number of steps is:
  //   vertices() co-ordinates
  // + vertices()+(exclude_alternate_colour ? 0 : vertices()) textures
  // + triangles_to_output triangles
  
  const uint steps=vertices()+vertices()+(exclude_alternate_colour ? 0 : vertices())+triangles_to_output;
  uint step=0;

  progress_start(100,"Writing POV-Ray file");

  const bool save_pov_mode=POVMode::pov_mode();
  POVMode::pov_mode(true);

  {
    std::ofstream out((fname_base+std::string(".pov")).c_str());
    
    // Boilerplate for renderer
    
    out << "#include \"colors.inc\"\n";
    out << "camera {perspective location <0,1,-4.5> look_at <0,0,0> angle 45}\n";
    out << "light_source {<100,100,-100> color White}\n";
    out << "#include \""+fname_base+"\".inc\"\n";
  }

  {
    // Use POV's mesh2 object
    std::ofstream out((fname_base+std::string(".inc")).c_str());

    out << header << "\n";
    
    out << "mesh2 {\n";
  
    // Output all the vertex co-ordinates
    out << "vertex_vectors {" << vertices() << ",\n";
    
    for (uint v=0;v<vertices();v++)
      {
	step++;
	progress_step((100*step)/steps);

	if (v!=0)
	  out << ",";
	out << vertex(v).position() << "\n";
      }
    out << "}\n";

    // Output the vertex colours
    // If exclude_alternate_colour is true, don't output the alternate colours
    out << "texture_list {" << vertices()+(exclude_alternate_colour ? 0 : vertices()) << "\n";
    
    for (uint c=0;c<(exclude_alternate_colour ? 1 : 2);c++)
      for (uint v=0;v<vertices();v++)
	{
	  step++;
	  progress_step((100*step)/steps);
	  
	  out << "texture{pigment{rgb " << FloatRGB(vertex(v).colour(c)) << "}}\n";
	}
    
    out << "}\n";
    
    out << "face_indices {" << triangles_to_output << ",\n";
    bool skip_initial_comma=true;
    for (uint t=0;t<triangles_to_output;t++)
      {
	step++;
	progress_step((100*step)/steps);

	if (skip_initial_comma)
	  skip_initial_comma=false;
	else
	  out << ",";

	out 
	  << "<" 
	  << triangle(t).vertex(0) 
	  << "," 
	  << triangle(t).vertex(1) 
	  << "," 
	  << triangle(t).vertex(2) 
	  << ">"; 

	out << "," << triangle(t).vertex(0)+(t<triangles_of_colour0() ? 0 : vertices());
	out << "," << triangle(t).vertex(1)+(t<triangles_of_colour0() ? 0 : vertices());
	out << "," << triangle(t).vertex(2)+(t<triangles_of_colour0() ? 0 : vertices());
	out << "\n";
      }
    out << "}\n";
    
    out << "}\n";
  }
  
  POVMode::pov_mode(save_pov_mode);

  progress_complete("Wrote POV-Ray file");
}

TriangleMeshFlatTriangle::TriangleMeshFlatTriangle(float z,uint seed,Progress* progress)
:TriangleMesh(progress)
 ,_geometry(seed)
{
  add_vertex(Vertex(XYZ(0.0,0.0,z)));

  for (uint i=0;i<6;i++)
    {
      add_vertex(Vertex(XYZ(cos(i*M_PI/3.0),sin(i*M_PI/3.0),z)));
    }

  for (uint i=0;i<6;i++)
    {
      add_triangle(Triangle(0,1+i,1+(i+1)%6));
    }
}

TriangleMeshIcosahedron::TriangleMeshIcosahedron(float radius,uint seed,Progress* progress)
:TriangleMesh(progress)
 ,_geometry(seed)
{
  const float x=0.525731112119133606;
  const float z=0.850650808352039932;
  
  const float vdata[12][3]=
    {
      { -x,0.0,  z},
      {  x,0.0,  z},
      { -x,0.0, -z},
      {  x,0.0, -z},
      {0.0,  z,  x},
      {0.0,  z, -x},
      {0.0, -z,  x},
      {0.0, -z, -x},
      {  z,  x,0.0},
      { -z,  x,0.0},
      {  z, -x,0.0},
      { -z, -x,0.0}
    };
  
  for (uint v=0;v<12;v++)
    add_vertex(Vertex(radius*XYZ(vdata[v][0],vdata[v][1],vdata[v][2]).normalised()));
  
  uint tindices[20][3]=
    {
      { 0, 4, 1},
      { 0, 9, 4},
      { 9, 5, 4},
      { 4, 5, 8},
      { 4, 8, 1},
      { 8,10, 1},
      { 8, 3,10},
      { 5, 3, 8},
      { 5, 2, 3},
      { 2, 7, 3},
      { 7,10, 3},
      { 7, 6,10},
      { 7,11, 6},
      {11, 0, 6},
      { 0, 1, 6},
      { 6, 1,10},
      { 9, 0,11},
      { 9,11, 2},
      { 9, 2, 5},
      { 7, 2,11}
    };

  for (uint t=0;t<20;t++)
    add_triangle(Triangle(tindices[t][2],tindices[t][1],tindices[t][0]));
}

TriangleMeshSubdividedIcosahedron::TriangleMeshSubdividedIcosahedron(float radius,uint subdivisions,uint flat_subdivisions,uint seed,const XYZ& variation,Progress* progress)
  :TriangleMesh(progress)
  ,TriangleMeshIcosahedron(radius,seed,progress)
{
  subdivide(subdivisions,flat_subdivisions,variation);
}

