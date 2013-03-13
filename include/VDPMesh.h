/*******************************************************************************
* CGoGN: Combinatorial and Geometric modeling with Generic N-dimensional Maps  *
* version 0.1                                                                  *
* Copyright (C) 2009-2012, IGG Team, LSIIT, University of Strasbourg           *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#ifndef __PMESH__
#define __PMESH__

#include "Algo/Decimation/selector.h"
#include "Algo/Decimation/edgeSelector.h"
#include "Algo/Decimation/geometryApproximator.h"
#include "Algo/Decimation/geometryPredictor.h"
#include "Algo/Decimation/colorPerVertexApproximator.h"

#include "Utils/quantization.h"
#include "Node.h"

namespace CGoGN
{

namespace Algo
{

namespace Surface
{

namespace VDPMesh
{

template <typename PFP>
class VDProgressiveMesh
{
public:
	typedef typename PFP::MAP MAP ;
	typedef typename PFP::VEC3 VEC3 ;
	typedef typename PFP::REAL REAL ;
    

private:
	MAP& m_map ;
	VertexAttribute<typename PFP::VEC3>& positionsTable ;
    
    typedef NoMathIOAttribute<Algo::Surface::VDPMesh::NodeInfo> EmbNode;
    VertexAttribute<EmbNode> noeud;

	DartMarker& inactiveMarker ;
	SelectorUnmarked dartSelect ;

	Algo::Surface::Decimation::EdgeSelector<PFP>* m_selector ;
	std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*> m_approximators ;
	std::list<VSplit<PFP>*> m_splits ;
    std::list<Node*> m_active_nodes;

	Algo::Surface::Decimation::Approximator<PFP, VEC3, EDGE>* m_positionApproximator ;

	bool m_initOk ;

    //DEBUG
    int m_height; //Hauteur de l'arbre le plus grand de la forêt
    std::list<Dart>* dart_transfo;

public:
	VDProgressiveMesh(
		MAP& map, DartMarker& inactive,
		VertexAttribute<typename PFP::VEC3>& position
	) ;
	~VDProgressiveMesh() ;

	bool initOk() { return m_initOk ; }

    void addNodes() ;
    bool areAdjacentFacesActive(Dart d) ;

	void createPM(unsigned int percentWantedVertices) ;

	std::vector<VSplit<PFP>*>& splits() { return m_splits ; }
	Algo::Surface::Decimation::EdgeSelector<PFP>* selector() { return m_selector ; }
	std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*>& approximators() { return m_approximators ; }

	void edgeCollapse(VSplit<PFP>* vs) ;
	void vertexSplit(VSplit<PFP>* vs) ;

	void coarsen() ;
	void refine() ;

	int coarsen(Node* n) ;
	int refine(Node* n) ;

	unsigned int nbSplits() { return m_splits.size() ; }

    /*DEBUG FUNCTIONS*/
    int getForestHeight() { return m_height; }
    void drawForest();
    void drawTree(Node* node);
} ;

} //namespace VDPMesh
} //namespace Surface
} //namespace Algo
} //namespace CGoGN

#include "VDPMesh.hpp"

#endif
