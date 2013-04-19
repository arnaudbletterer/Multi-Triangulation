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

#include "Algo/Geometry/localFrame.h"

namespace CGoGN
{

namespace Algo
{

namespace Surface
{

namespace VDPMesh
{
    
template <typename PFP>
VDProgressiveMesh<PFP>::VDProgressiveMesh(
		MAP& map, DartMarker& inactive,
		VertexAttribute<typename PFP::VEC3>& position,
		Geom::BoundingBox<typename PFP::VEC3> bb
	) :
	m_map(map), positionsTable(position), inactiveMarker(inactive), dartSelect(inactiveMarker), m_height(0)
{
	CGoGNout << "  creating approximator .." << CGoGNflush ;
	std::vector<VertexAttribute< typename PFP::VEC3>* > pos_v ;
	pos_v.push_back(&positionsTable) ;
	m_approximators.push_back(new Algo::Surface::Decimation::Approximator_MidEdge<PFP>(m_map, pos_v)) ;
	CGoGNout << "..done" << CGoGNendl ;


	CGoGNout << "  creating selector.." << CGoGNflush ;			
    m_selector = new Algo::Surface::Decimation::EdgeSelector_Length<PFP>(m_map, positionsTable, m_approximators, dartSelect) ;	
    CGoGNout << "..done" << CGoGNendl ;

	m_initOk = true ;

	CGoGNout << "  initializing approximators.." << CGoGNflush ;
	for(typename std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*>::iterator it = m_approximators.begin(); it != m_approximators.end(); ++it)
	{
		if(! (*it)->init())
			m_initOk = false ;
		if((*it)->getApproximatedAttributeName() == "position")
			m_positionApproximator = reinterpret_cast<Algo::Surface::Decimation::Approximator<PFP, VEC3, EDGE>*>(*it) ;
	}
	CGoGNout << "..done" << CGoGNendl ;

	CGoGNout << "  initializing selector.." << CGoGNflush ;
	m_initOk = m_selector->init() ;
    CGoGNout << "..done" << CGoGNendl ;
    
    noeud = m_map.template getAttribute<EmbNode, VERTEX>("noeud") ;
	if(!noeud.isValid())
		noeud = m_map.template addAttribute<EmbNode, VERTEX>("noeud") ;

    CGoGNout << "  initializing drawer.." << CGoGNflush;
    m_bb = new Box(bb);
	m_bb->updateDrawer();
	updateRefinement();
    CGoGNout << "..done" << CGoGNendl;
}

template <typename PFP>
VDProgressiveMesh<PFP>::~VDProgressiveMesh()
{
	m_active_nodes.clear();
    if(m_selector)
		delete m_selector ;
	for(typename std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*>::iterator it = m_approximators.begin(); it != m_approximators.end(); ++it)
		delete (*it) ;
	delete m_bb;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::addNodes() {
    TraversorV<MAP> trav(m_map);
    for(Dart d = trav.begin(); d!=trav.end(); d = trav.next()) {
        noeud[d].node = new Node(NULL, true, m_map.template getEmbedding<VERTEX>(d), 0);
        m_active_nodes.push_front(noeud[d].node);
        noeud[d].node->setCurrentPosition(m_active_nodes.begin());
    }
    m_height = 0;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::createPM(unsigned int percentWantedVertices)
{
	unsigned int nbVertices = m_map.template getNbOrbits<VERTEX>() ;
	unsigned int nbWantedVertices = nbVertices * percentWantedVertices / 100 ;
    
    CGoGNout << "  initializing nodes.." << CGoGNflush ;
    addNodes();
	CGoGNout << "..done" << CGoGNendl ;
	
    CGoGNout << "  creating PM (" << nbVertices << " vertices).." << /* flush */ CGoGNflush ;
    
	bool finished = false ;
	Dart d ;
	while(!finished)
	{
		if(!m_selector->nextEdge(d))
			break ;

		--nbVertices ;
		Dart d1 = m_map.phi2(m_map.phi1(d)) ;
		Dart d2 = m_map.phi2(m_map.phi_1(d)) ;
	    Dart dd1 = m_map.phi2(m_map.phi1(m_map.phi2(d))) ;
		Dart dd2 = m_map.phi2(m_map.phi_1(m_map.phi2(d))) ;

		VSplit<PFP>* vs = new VSplit<PFP>(m_map, d, dd2, d2, dd1, d1) ;	// create new VSplit node 
        
        Node* n = new Node(vs);   //Création du nouveau noeud de l'arbre

        Node* n_d2 = noeud[d2].node;
        Node* n_dd2 = noeud[dd2].node;

        /*Mise en place de la hiérarchie par rapport au nouveau noeud*/
        n->setLeftChild(n_d2);
        n->setRightChild(n_dd2);
        n_d2->setParent(n);
        n_dd2->setParent(n);

        /*Calcul de la hauteur du plus grand arbre de la forêt*/
        int height;
        if(noeud[d2].node->getHeight()>noeud[dd2].node->getHeight()) {
            height = noeud[d2].node->getHeight();
        }
        else {
            height = noeud[dd2].node->getHeight();
        }

        /*Suppression des deux anciens noeuds du front courant*/
        m_active_nodes.erase(n_d2->getCurrentPosition());
        m_active_nodes.erase(n_dd2->getCurrentPosition());
        n_d2->setActive(false);
        n_dd2->setActive(false);
        
        /*Ajout du nouveau noeud au front courant*/
        n->setActive(true);
        m_active_nodes.push_back(n);
        n->setCurrentPosition(--m_active_nodes.end());
        
        n->setHeight(height+1);
        
        if(m_height<=height) {
            /*Si la hauteur du plus grand arbre est inférieure ou égale à celle de l'arbre actuel*/
            /*NB: La hauteur du plus grand arbre ne sera jamais inférieure à celle de l'arbre courant*/
            ++m_height;
        }

		for(typename std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*>::iterator it = m_approximators.begin(); it != m_approximators.end(); ++it)
		{
			(*it)->approximate(d) ;					// compute approximated attributes with its associated detail
			(*it)->saveApprox(d) ;
		}

		m_selector->updateBeforeCollapse(d) ;		// update selector

		edgeCollapse(n->getVSplit()) ;							// collapse edge

		unsigned int newV = m_map.template setOrbitEmbeddingOnNewCell<VERTEX>(d2) ;
		unsigned int newE1 = m_map.template setOrbitEmbeddingOnNewCell<EDGE>(d2) ;
		unsigned int newE2 = m_map.template setOrbitEmbeddingOnNewCell<EDGE>(dd2) ;
		n->getVSplit()->setApproxV(newV) ;
		n->getVSplit()->setApproxE1(newE1) ;
		n->getVSplit()->setApproxE2(newE2) ;
        
        noeud[d2].node = n; //Affectation du nouveau noeud a l'attribut de sommet
        n->setVertex(m_map.template getEmbedding<VERTEX>(d2));  //Indique le numéro de sommet pointant sur ce noeud
        
		for(typename std::vector<Algo::Surface::Decimation::ApproximatorGen<PFP>*>::iterator it = m_approximators.begin(); it != m_approximators.end(); ++it)
			(*it)->affectApprox(d2);				// affect data to the resulting vertex

		m_selector->updateAfterCollapse(d2, dd2) ;	// update selector

		if(nbVertices <= nbWantedVertices)
			finished = true ;

	}
	delete m_selector ;
	m_selector = NULL ;

	CGoGNout << "..done (" << nbVertices << " vertices)" << CGoGNendl ;
    CGoGNout << m_active_nodes.size() << " active nodes" << CGoGNendl;
    CGoGNout << "Hauteur du plus grand arbre de la forêt : " << m_height << CGoGNendl;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::edgeCollapse(VSplit<PFP>* vs)
{
	Dart d = vs->getEdge() ;
	Dart dd = m_map.phi2(d) ;

	inactiveMarker.markOrbit<FACE>(d) ;
	inactiveMarker.markOrbit<FACE>(dd) ;

	m_map.extractTrianglePair(d) ;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::vertexSplit(VSplit<PFP>* vs)
{
	Dart d = vs->getEdge() ;
	Dart dd = m_map.phi2(d) ;
	Dart d2 = vs->getLeftEdge() ;
	Dart dd2 = vs->getRightEdge() ;
            
	m_map.insertTrianglePair(d, d2, dd2) ;

	inactiveMarker.unmarkOrbit<FACE>(d) ;
	inactiveMarker.unmarkOrbit<FACE>(dd) ;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::coarsen() {
    CGoGNout << "COARSEN" << CGoGNendl;
    std::list<Node*>::iterator it;
    std::list<Node*>::iterator it_back;
    int compteur = 0;
    bool stop = false;
    while(!stop) {
    	it=m_active_nodes.begin();
		while(it != m_active_nodes.end()) {
			it_back = it;
			std::advance(it_back, 2);
			if(coarsen(*it)==1) {
				//Une modification a été faite
				if(it != --m_active_nodes.end()) {
					it = it_back;
				}
				++compteur;
			}
			else {
				++it;
			}
		}
		if(compteur==0) {
			//Aucune modification n'a été faite, état stationnaire
			//Arrêt de la simplification du maillage
			stop = true;
		}
		else {
			//Au moins une modification a été faite
			//On recommence la simplification du maillage
			compteur = 0;
		}
    }
    drawFront();
}

template <typename PFP>
int VDProgressiveMesh<PFP>::coarsen(Node* n)
{
    int res = 0;
    if(n && n->isActive()) {
        //Si n fait partie du front et qu'il n'est pas contenu dans la boîte d'intérêt
        Node* parent = n->getParent();
        if(parent && !parent->isActive()) {
            /*Si le noeud parent existe et qu'il ne fait pas partie du front*/
            Node* child_left = parent->getLeftChild();
            Node* child_right = parent->getRightChild();
            if(child_left && child_left->isActive()
            && child_right && child_right->isActive()) {
                //Si n a un noeud parent et que celui-ci ne fait pas partie du front
                VSplit<PFP>* vs = parent->getVSplit(); 
                Dart d2 = vs->getLeftEdge();
                Dart dd2 = vs->getRightEdge();
                Dart d1 = vs->getOppositeLeftEdge();
                Dart dd1 = vs->getOppositeRightEdge();

                if( inactiveMarker.isMarked(d1)
                ||  inactiveMarker.isMarked(d2)
                ||  inactiveMarker.isMarked(dd1)
                ||  inactiveMarker.isMarked(dd2))
                    return res;

                edgeCollapse(vs);

                m_map.template setOrbitEmbedding<VERTEX>(d2, vs->getApproxV());
	            m_map.template setOrbitEmbedding<EDGE>(d2, vs->getApproxE1());
                m_map.template setOrbitEmbedding<EDGE>(dd2, vs->getApproxE2());
        
                if(m_map.template getEmbedding<VERTEX>(d2) != m_map.template getEmbedding<VERTEX>(dd2)) {
                	CGoGNout << "Pas bon pas bon coarsen" << CGoGNendl;
                	CGoGNout << "  D2 : " << m_map.template getEmbedding<VERTEX>(d2) << CGoGNendl;
                	CGoGNout << "  DD2 : " << m_map.template getEmbedding<VERTEX>(dd2) << CGoGNendl;
                	return res;
                }

                //Mise a jour des informations de l'arbre
                m_active_nodes.erase(child_left->getCurrentPosition());
                m_active_nodes.erase(child_right->getCurrentPosition());
                child_left->setActive(false);
                child_right->setActive(false);
                parent->setActive(true);
                m_active_nodes.push_back(parent);
                parent->setCurrentPosition(--m_active_nodes.end());
                ++res;
            }
        }
    }
    return res;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::refine() {
    CGoGNout << "REFINE" << CGoGNendl;
    std::list<Node*>::iterator it_back;
    std::list<Node*>::iterator it;
    int compteur = 0;
    bool stop = false;
    while(!stop) {
    	it=m_active_nodes.begin();
		while(it!=m_active_nodes.end()) {
			it_back = it;
			++it_back;
			if(refine(*it)==1) {
				//Une modification a été faite
				++compteur;
			}
			it = it_back;
		}
		if(compteur==0) {
			//Aucune modification n'a été faite, état stationnaire
			//Arrêt de l'affinement du maillage
			stop = true;
		}
		else {
			//Au moins une modification a été faite
			//On recommence l'affinement du maillage
			compteur = 0;
		}
    }
    drawFront();
}

template <typename PFP>
int VDProgressiveMesh<PFP>::refine(Node* n)
{
    int res = 0;
    if(n && n->isActive()) {
        //Si n fait partie du front
        Node* child_left = n->getLeftChild();
        Node* child_right = n->getRightChild();
        if( child_left && !child_left->isActive() 
        &&  child_right && !child_right->isActive()) {
            //Si n a deux fils et que ceux-ci ne font pas partie du front
            VSplit<PFP>* vs = n->getVSplit();

	        Dart d = vs->getEdge();
	        Dart dd = m_map.phi2(d);
	        Dart dd2 = vs->getRightEdge();
	        Dart d2 = vs->getLeftEdge();
	        Dart d1 = vs->getOppositeLeftEdge();
	        Dart dd1 = vs->getOppositeRightEdge();

	        if(m_map.phi2(d1)!=d2 || m_map.phi2(dd1)!=dd2) {
	        	CGoGNout << "Mais c'est pas les mêmes" << CGoGNendl;
	        	return res;
	        }

	        //Vérification de la présence des brins entourant la paire de triangles
            if( inactiveMarker.isMarked(d1)
            ||  inactiveMarker.isMarked(d2)
            ||  inactiveMarker.isMarked(dd1)
            ||  inactiveMarker.isMarked(dd2)) {
                return res;
            }

            //Vérification
            if(m_map.template getEmbedding<VERTEX>(d2) != m_map.template getEmbedding<VERTEX>(dd2)) {
            	CGoGNout << "Pas bon pas bon refine" << CGoGNendl;
            	CGoGNout << "  D2 : " << m_map.template getEmbedding<VERTEX>(d2) << CGoGNendl;
            	CGoGNout << "  DD2 : " << m_map.template getEmbedding<VERTEX>(dd2) << CGoGNendl;
            	return res;
            }

	        unsigned int v1 = m_map.template getEmbedding<VERTEX>(d);				// get the embedding
	        unsigned int v2 = m_map.template getEmbedding<VERTEX>(dd);			// of the new vertices
	        unsigned int e1 = m_map.template getEmbedding<EDGE>(m_map.phi1(d));
	        unsigned int e2 = m_map.template getEmbedding<EDGE>(m_map.phi_1(d));	// and new edges
	        unsigned int e3 = m_map.template getEmbedding<EDGE>(m_map.phi1(dd));
	        unsigned int e4 = m_map.template getEmbedding<EDGE>(m_map.phi_1(dd));
	
            vertexSplit(vs);
            
	        m_map.template setOrbitEmbedding<VERTEX>(d, v1);		// embed the
	        m_map.template setOrbitEmbedding<VERTEX>(dd, v2);	// new vertices
	        m_map.template setOrbitEmbedding<EDGE>(d1, e1);
	        m_map.template setOrbitEmbedding<EDGE>(d2, e2);		// and new edges
	        m_map.template setOrbitEmbedding<EDGE>(dd1, e3);
	        m_map.template setOrbitEmbedding<EDGE>(dd2, e4);

            m_map.template copyDartEmbedding<VERTEX>(m_map.phi_1(d), d1);
            m_map.template copyDartEmbedding<VERTEX>(m_map.phi_1(dd), dd1);

            //Mise a jour des informations de l'arbre
            m_active_nodes.erase(n->getCurrentPosition());
            n->setActive(false);
            child_left->setActive(true);
            child_right->setActive(true);
            m_active_nodes.push_back(child_left);
            child_left->setCurrentPosition(--m_active_nodes.end());
            m_active_nodes.push_back(child_right);
            child_right->setCurrentPosition(--m_active_nodes.end());
            res++;
        }
    }
    return res;
}

template <typename PFP>
void VDProgressiveMesh<PFP>::updateRefinement() {
	std::list<Node*>::iterator it = m_active_nodes.begin();
	std::list<Node*>::iterator it_back;
	bool stop = false;
	int compteur = 0;
	bool transformation = false;
	while(!stop) {
		it = m_active_nodes.begin();
		while(it != m_active_nodes.end()) {
			transformation = false;
			if(it != m_active_nodes.begin()) {
				it_back = it;	//On retient l'élément prédécesseur
				--it_back;
			}
			else {
				//Si l'élément en cours de traitement est le premier de la liste
				it_back = m_active_nodes.end();
			}

			if((*it) && (*it)->isActive()) {
				//Si le noeud est actuellement affichéq
				if(m_bb->contains(positionsTable[(*it)->getVertex()])) {
					//Si le noeud appartient à la boîte d'intérêt
					if(refine(*it)==1) {
						//Si l'opération a été effectuée = des éléments ont été supprimés
						transformation = true;
						++compteur;
					}
				}
				else {
					//Si le noeud n'appartient pas à la boîte d'intérêt
					if((*it)->getParent()) {
						Node* child_left = (*it)->getParent()->getLeftChild();
						Node* child_right = (*it)->getParent()->getRightChild();
						if(		!m_bb->contains(positionsTable[(*it)->getParent()->getVertex()])
							&&	!m_bb->contains(positionsTable[child_left->getVertex()])
							&& 	!m_bb->contains(positionsTable[child_right->getVertex()])) {
							//Si le noeud a un parent qui n'appartient pas à la boîte d'intérêt
							if(coarsen(*it) ==1) {
								//Si l'opération a été effectuée = des éléments ont été supprimés
								transformation = true;
								++compteur;
							}
						}
					}
				}
			}

			if(transformation) {
				//Si une transformation a eu lieu = des élément ont été supprimés
				if(it_back != m_active_nodes.end()) {
					it = ++it_back;
				}
				else {
					it = m_active_nodes.begin();
				}
			}
			else {
				//Si aucune transformation n'a eu lieu
				std::advance(it_back, 2);	//On saute l'élément suivant, qui vient d'être traité à l'instant
				it = it_back;
			}
		}
		if(compteur == 0) {
			stop = true;
		}
		else {
			compteur = 0;
		}
	}
}

template <typename PFP>
bool VDProgressiveMesh<PFP>::isEdgeCollapseLegal(Node* n) {
	bool res = false;
	if(n->getParent()) {
		Node* child_left = n->getParent()->getLeftChild();
		Node* child_right = n->getParent()->getRightChild();
		if(child_left && child_right && child_left->isActive() && child_right->isActive()) {
			VSplit<PFP>* vs = n->getVSplit();
			if(vs) {
				Dart dd2 = vs->getRightEdge();
				Dart d2 = vs->getLeftEdge();
				Dart d1 = vs->getOppositeLeftEdge();
				Dart dd1 = vs->getOppositeRightEdge();

				if( !inactiveMarker.isMarked(d1)
				&&  !inactiveMarker.isMarked(d2)
				&&  !inactiveMarker.isMarked(dd1)
				&&  !inactiveMarker.isMarked(dd2))
					res = true;
			}
		}
	}
	return res;
}

template <typename PFP>
bool VDProgressiveMesh<PFP>::isVertexSplitLegal(Node* n) {
	bool res = false;
	if(n->isActive()) {
		VSplit<PFP>* vs = n->getVSplit();
		if(vs) {
			Dart dd2 = vs->getRightEdge();
			Dart d2 = vs->getLeftEdge();
			Dart d1 = vs->getOppositeLeftEdge();
			Dart dd1 = vs->getOppositeRightEdge();

			if( !inactiveMarker.isMarked(d1)
			&&  !inactiveMarker.isMarked(d2)
			&&  !inactiveMarker.isMarked(dd1)
			&&  !inactiveMarker.isMarked(dd2))
				res = true;
		}
	}
	return res;
}

/*FONCTIONS DE DEBOGAGE*/
template <typename PFP>
void VDProgressiveMesh<PFP>::drawForest() {
    for(std::list<Node*>::iterator it = m_active_nodes.begin(); it != m_active_nodes.end(); ++it) {
        /*On parcourt l'ensemble des racines de la forêt*/
        drawTree(*it);
        CGoGNout << CGoGNendl;
    } 
}

template <typename PFP>
void VDProgressiveMesh<PFP>::drawTree(Node* node) {
    CGoGNout << node->getVertex() << CGoGNflush;
    if(node) {
        if(node->getLeftChild()) {
            CGoGNout << " G( " << CGoGNflush;
            drawTree(node->getLeftChild());
            CGoGNout << " ) " << CGoGNflush;
        }
        if(node->getRightChild()) {
            CGoGNout << " D( " << CGoGNflush;
            drawTree(node->getRightChild());
            CGoGNout << " ) " << CGoGNflush;
        }
    }
}

template <typename PFP>
void VDProgressiveMesh<PFP>::drawFront() {
    CGoGNout << "Front courant : " << CGoGNendl;
    CGoGNout << "  " << m_active_nodes.size() << " noeuds actifs" << CGoGNendl;
    for(std::list<Node*>::iterator it = m_active_nodes.begin(); it != m_active_nodes.end(); ++it) {
        if(it!=--m_active_nodes.end())
            CGoGNout << (*it)->getVertex() << " | " << CGoGNflush;
        else
            CGoGNout << (*it)->getVertex() <<  CGoGNendl;
    }
}

} // namespace VDPMesh
} // namespace Surface
} // namespace Algo
} // namespace CGoGN
