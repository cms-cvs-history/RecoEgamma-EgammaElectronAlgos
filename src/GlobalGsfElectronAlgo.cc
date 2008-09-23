// -*- C++ -*-
//
// Package:    EgammaElectronAlgos
// Class:      GlobalGsfElectronAlgo
// 
/**\class GlobalGsfElectronAlgo EgammaElectronAlgos/GlobalGsfElectronAlgo

 Description: top algorithm producing TrackCandidate and Electron objects from supercluster
              driven pixel seeded Ckf tracking
 
*/
//
// Original Author:  Ursula Berthon, Claude Charlot
//         Created:  Thu july 6 13:22:06 CEST 2006
// $Id: GlobalGsfElectronAlgo.cc,v 1.2 2008/05/21 13:10:46 uberthon Exp $
//
//

#include "DataFormats/GsfTrackReco/interface/GsfTrackFwd.h"

#include "RecoEgamma/EgammaElectronAlgos/interface/GlobalGsfElectronAlgo.h"
#include "RecoEgamma/EgammaElectronAlgos/interface/ElectronClassification.h"
#include "RecoEgamma/EgammaElectronAlgos/interface/ElectronMomentumCorrector.h"
#include "RecoEgamma/EgammaElectronAlgos/interface/ElectronEnergyCorrector.h"
#include "RecoEgamma/EgammaTools/interface/ECALPositionCalculator.h"
#include "RecoEgamma/EgammaTools/interface/HoECalculator.h"

#include "DataFormats/EgammaReco/interface/BasicCluster.h"
#include "DataFormats/EgammaReco/interface/ElectronPixelSeed.h"
#include "DataFormats/EgammaReco/interface/ElectronPixelSeedFwd.h"
#include "DataFormats/TrackCandidate/interface/TrackCandidate.h"
#include "DataFormats/TrackCandidate/interface/TrackCandidateCollection.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "DataFormats/TrackingRecHit/interface/TrackingRecHitFwd.h"
#include "DataFormats/HcalDetId/interface/HcalSubdetector.h"
#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "DataFormats/HcalRecHit/interface/HcalRecHitCollections.h"
#include "DataFormats/EcalDetId/interface/EcalSubdetector.h"

#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"


#include "TrackingTools/PatternTools/interface/Trajectory.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"
#include "TrackingTools/PatternTools/interface/TransverseImpactPointExtrapolator.h"
#include "TrackingTools/PatternTools/interface/TSCPBuilderNoMaterial.h"
#include "TrackingTools/GsfTools/interface/MultiTrajectoryStateTransform.h"
#include "TrackingTools/GsfTools/interface/GSUtilities.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"
#include "TrackingTools/GeomPropagators/interface/AnalyticalPropagator.h"
#include "TrackingTools/GsfTools/interface/GsfPropagatorAdapter.h"
#include "TrackingTools/GsfTools/interface/MultiGaussianStateTransform.h"
#include "TrackingTools/GsfTools/interface/MultiGaussianState1D.h"
#include "TrackingTools/GsfTools/interface/GaussianSumUtilities1D.h"
#include "RecoCaloTools/Selectors/interface/CaloConeSelector.h"

#include "RecoTracker/Record/interface/TrackerRecoGeometryRecord.h"
#include "RecoTracker/Record/interface/CkfComponentsRecord.h"

#include "Geometry/CommonDetUnit/interface/TrackingGeometry.h"
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "DataFormats/GeometryVector/interface/GlobalPoint.h"
#include "DataFormats/GeometryVector/interface/GlobalVector.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "CLHEP/Units/PhysicalConstants.h"
#include <TMath.h>
#include <sstream>
#include <Math/VectorUtil.h>
#include <Math/Point3D.h>


using namespace edm;
using namespace std;
using namespace reco;
//using namespace math; // conflicts with DataFormat/Math/interface/Point3D.h!!!!

GlobalGsfElectronAlgo::GlobalGsfElectronAlgo(const edm::ParameterSet& conf,
                                               double maxEOverPBarrel, double maxEOverPEndcaps, 
                                               double minEOverPBarrel, double minEOverPEndcaps,
                                               double maxDeltaEta, double maxDeltaPhi, 
					       bool highPtPresel, double highPtMin,
   				               bool applyEtaCorrection, bool applyAmbResolution):  
  maxEOverPBarrel_(maxEOverPBarrel), maxEOverPEndcaps_(maxEOverPEndcaps), 
  minEOverPBarrel_(minEOverPBarrel), minEOverPEndcaps_(minEOverPEndcaps), 
  maxDeltaEta_(maxDeltaEta), maxDeltaPhi_(maxDeltaPhi),
  highPtPreselection_(highPtPresel), highPtMin_(highPtMin),
  applyEtaCorrection_(applyEtaCorrection), applyAmbResolution_(applyAmbResolution),
  cacheIDGeom_(0),cacheIDTDGeom_(0),cacheIDMagField_(0)
{   
 // this is the new version allowing to configurate the algo
  // interfaces still need improvement!!
  mtsTransform_ = new MultiTrajectoryStateTransform;
  geomPropBw_=0;	
  geomPropFw_=0;	

  // get nested parameter set for the TransientInitialStateEstimator
  ParameterSet tise_params = conf.getParameter<ParameterSet>("TransientInitialStateEstimatorParameters") ;
  
  // get input collections
  hcalRecHits_ = conf.getParameter<edm::InputTag>("hcalRecHits");
  tracks_ = conf.getParameter<edm::InputTag>("tracks");  
  barrelSuperClusters_ = conf.getParameter<edm::InputTag>("barrelSuperClusters");
  endcapSuperClusters_ = conf.getParameter<edm::InputTag>("endcapSuperClusters");
}

GlobalGsfElectronAlgo::~GlobalGsfElectronAlgo() {
  delete geomPropBw_;
  delete geomPropFw_;
  delete mtsTransform_;
}

void GlobalGsfElectronAlgo::setupES(const edm::EventSetup& es) {

  // get EventSetupRecords if needed
  if (cacheIDMagField_!=es.get<IdealMagneticFieldRecord>().cacheIdentifier()){
    cacheIDMagField_=es.get<IdealMagneticFieldRecord>().cacheIdentifier();
    es.get<IdealMagneticFieldRecord>().get(theMagField);
    if (geomPropBw_) delete geomPropBw_;
    geomPropBw_ = new GsfPropagatorAdapter(AnalyticalPropagator(theMagField.product(), oppositeToMomentum));
    if (geomPropFw_) delete geomPropFw_;
    geomPropFw_ = new GsfPropagatorAdapter(AnalyticalPropagator(theMagField.product(), alongMomentum));
  }
  if (cacheIDTDGeom_!=es.get<TrackerDigiGeometryRecord>().cacheIdentifier()){
    cacheIDTDGeom_=es.get<TrackerDigiGeometryRecord>().cacheIdentifier();
    es.get<TrackerDigiGeometryRecord>().get(trackerHandle_);
  }

  if (cacheIDGeom_!=es.get<CaloGeometryRecord>().cacheIdentifier()){
    cacheIDGeom_=es.get<CaloGeometryRecord>().cacheIdentifier();
    es.get<CaloGeometryRecord>().get(theCaloGeom);
  }
  

}

void  GlobalGsfElectronAlgo::run(Event& e, GsfElectronCollection & outEle) {

  // get the input 
  edm::Handle<GsfTrackCollection> tracksH;
  e.getByLabel(tracks_,tracksH);
  
  // for HoE calculation
  edm::Handle<HBHERecHitCollection> hbhe;
  mhbhe_=0;
  bool got = e.getByLabel(hcalRecHits_,hbhe);  
  if (got) mhbhe_=  new HBHERecHitMetaCollection(*hbhe);

  // get the beamspot from the Event:
  edm::Handle<reco::BeamSpot> recoBeamSpotHandle;
  e.getByType(recoBeamSpotHandle);
  const math::XYZPoint bsPosition = recoBeamSpotHandle->position();

  // temporay array for electron before amb. solving
  std::vector<GsfElectron> tempEle;
  
  // create electrons 
  edm::Handle<SuperClusterCollection> superClustersBarrelH; 
  e.getByLabel(barrelSuperClusters_,superClustersBarrelH);
  
  edm::Handle<SuperClusterCollection> superClustersEndcapH; 
  e.getByLabel(endcapSuperClusters_, superClustersEndcapH);

  process(tracksH, 
          superClustersBarrelH, 
          superClustersEndcapH,   
	  bsPosition,
          tempEle);
 
  std::ostringstream str;

  str << "\n========== GlobalGsfElectronAlgo Info (before amb. solving) ==========";
  str << "\nEvent " << e.id();
  str << "\nNumber of final electron tracks: " << tracksH.product()->size();
  str << "\nNumber of final electrons: " << tempEle.size();
  for (vector<GsfElectron>::const_iterator it = tempEle.begin(); it != tempEle.end(); it++) {
    str << "\nNew electron with charge, pt, eta, phi : "  << it->charge() << " , " 
        << it->pt() << " , " << it->eta() << " , " << it->phi();
  }
 
  str << "\n=================================================";
  LogDebug("GlobalGsfElectronAlgo") << str.str();

  std::ostringstream str2;

  if (applyAmbResolution_) {
  
    resolveElectrons(tempEle, outEle);

    str2 << "\n========== GlobalGsfElectronAlgo Info (after amb. solving) ==========";
    str2 << "\nEvent " << e.id();
    str2 << "\nNumber of final electron tracks: " << tracksH.product()->size();
    str2 << "\nNumber of final electrons: " << outEle.size();
    for (vector<GsfElectron>::const_iterator it = outEle.begin(); it != outEle.end(); it++) {
      str2 << "\nNew electron with charge, pt, eta, phi : "  << it->charge() << " , " 
          << it->pt() << " , " << it->eta() << " , " << it->phi();
    }

    str2 << "\n=================================================";
    LogDebug("GlobalGsfElectronAlgo") << str2.str();

  } else {
  
    outEle = tempEle;
  
  }
  
  delete mhbhe_;
  return;
}

bool GlobalGsfElectronAlgo::preSelection(const SuperCluster& clus)
{

  LogDebug("")<< "========== preSelection ==========";

  double rt2 = clus.x()*clus.x() + clus.y()*clus.y();
  double r2 = rt2 + clus.z()*clus.z();
  double Et =clus.energy()*sqrt(rt2/r2);

  // pt min
  LogDebug("") << "pT : " << vtxMom_.perp();

  // E/p cut
  LogDebug("") << "E/p : " << clus.energy()/vtxMom_.mag();

  // no E/p preselection for high pT electrons
  if (!highPtPreselection_ || Et <= highPtMin_) {
    if ((subdet_==EcalBarrel) && (clus.energy()/vtxMom_.mag() > maxEOverPBarrel_)) return false;
    if ((subdet_==EcalEndcap) && (clus.energy()/vtxMom_.mag() > maxEOverPEndcaps_)) return false;
    if ((subdet_==EcalBarrel) && (clus.energy()/vtxMom_.mag() < minEOverPBarrel_)) return false;
    if ((subdet_==EcalEndcap) && (clus.energy()/vtxMom_.mag() < minEOverPEndcaps_)) return false;
  }
  LogDebug("") << "E/p criteria is satisfied ";

  // delta eta criteria
  double etaclu = clus.eta();
  double etatrk = sclPos_.eta();
  double deta = etaclu-etatrk;
  LogDebug("") << "delta eta : " << deta;
  if (fabs(deta) > maxDeltaEta_) return false;
  LogDebug("") << "Delta eta criteria is satisfied ";

  // delta phi criteria
  double phiclu = clus.phi();
  double phitrk = sclPos_.phi();
  double dphi = phiclu-phitrk;
  if (fabs(dphi)>CLHEP::pi)
    dphi = dphi < 0? (CLHEP::twopi) + dphi : dphi - CLHEP::twopi;
  LogDebug("") << "delta phi : " << dphi;
  if (fabs(dphi) > maxDeltaPhi_) return false;
  LogDebug("") << "Delta phi criteria is satisfied ";

  LogDebug("") << "electron has passed preselection criteria ";
  LogDebug("") << "=================================================";
  return true;  

}  

GlobalVector GlobalGsfElectronAlgo::computeMode(const TrajectoryStateOnSurface &tsos) {

  // mode computation for momentum cartesian co-ordinates
  // change to 5D in local parameters??
  float mode_Px = 0.;
  float mode_Py = 0.;
  float mode_Pz = 0.;
  if ( tsos.isValid() ){
    std::vector<TrajectoryStateOnSurface> components(tsos.components());
    unsigned int numb = components.size();
    std::vector<SingleGaussianState1D> pxStates; pxStates.reserve(numb);
    std::vector<SingleGaussianState1D> pyStates; pyStates.reserve(numb);
    std::vector<SingleGaussianState1D> pzStates; pzStates.reserve(numb);
    for ( std::vector<TrajectoryStateOnSurface>::const_iterator ic=components.begin();
	  ic!=components.end(); ++ic ) {
      GlobalVector momentum(ic->globalMomentum());
      AlgebraicSymMatrix66 cov(ic->cartesianError().matrix());
      pxStates.push_back(SingleGaussianState1D(momentum.x(),cov(3,3),ic->weight()));
      pyStates.push_back(SingleGaussianState1D(momentum.y(),cov(4,4),ic->weight()));
      pzStates.push_back(SingleGaussianState1D(momentum.z(),cov(5,5),ic->weight()));
    }
    MultiGaussianState1D pxState(pxStates);
    MultiGaussianState1D pyState(pyStates);
    MultiGaussianState1D pzState(pzStates);
    GaussianSumUtilities1D pxUtils(pxState);
    GaussianSumUtilities1D pyUtils(pyState);
    GaussianSumUtilities1D pzUtils(pzState);
    mode_Px = pxUtils.mode().mean();
    mode_Py = pyUtils.mode().mean();
    mode_Pz = pzUtils.mode().mean();
  } else edm::LogInfo("") << "tsos not valid!!";
  return GlobalVector(mode_Px,mode_Py,mode_Pz);	

}

// interface to be improved...
void GlobalGsfElectronAlgo::createElectron(const SuperClusterRef & scRef, const GsfTrackRef &trackRef, GsfElectronCollection & outEle) {
      GlobalVector innMom=computeMode(innTSOS_);
      GlobalPoint innPos=innTSOS_.globalPosition();
      GlobalVector seedMom=computeMode(seedTSOS_);
      GlobalPoint  seedPos=seedTSOS_.globalPosition();
      GlobalVector sclMom=computeMode(sclTSOS_);    

      GlobalPoint  vtxPos=vtxTSOS_.globalPosition();
      GlobalVector outMom=computeMode(outTSOS_);
      GlobalPoint  outPos=outTSOS_.globalPosition();

      //create electron
      double scale = (*scRef).energy()/vtxMom_.mag();    
      math::XYZTLorentzVectorD momentum= math::XYZTLorentzVector(vtxMom_.x()*scale,
								 vtxMom_.y()*scale,
								 vtxMom_.z()*scale,
								 (*scRef).energy());
      // should be coming from supercluster!
     HoECalculator calc(theCaloGeom);
     double HoE=calc(&(*scRef),mhbhe_);
     GsfElectron ele(momentum,scRef,trackRef,sclPos_,sclMom,seedPos,seedMom,innPos,innMom,vtxPos,vtxMom_,outPos,outMom,HoE);

      //and set various properties
      ECALPositionCalculator ecpc;
      float trackEta=ecpc.ecalEta(trackRef->innerMomentum(),trackRef->innerPosition());
      float trackPhi=ecpc.ecalPhi(theMagField.product(), trackRef->innerMomentum(),trackRef->innerPosition(),trackRef->charge());

      ele.setDeltaEtaSuperClusterAtVtx((*scRef).position().eta() - trackEta);
      float dphi = (*scRef).position().phi() - trackPhi;
      if (fabs(dphi)>CLHEP::pi)
	dphi = dphi < 0? CLHEP::twopi + dphi : dphi - CLHEP::twopi;
      ele.setDeltaPhiSuperClusterAtVtx(dphi);

      // set corrections + classification
      ElectronClassification theClassifier;
      theClassifier.correct(ele);
      ElectronEnergyCorrector theEnCorrector;
      theEnCorrector.correct(ele, applyEtaCorrection_);
      ElectronMomentumCorrector theMomCorrector;
      theMomCorrector.correct(ele,vtxTSOS_);


      outEle.push_back(ele);
}

bool  GlobalGsfElectronAlgo::calculateTSOS(const GsfTrack &t,const SuperCluster & theClus, const math::XYZPoint &
bsPosition){

    //at innermost point
    innTSOS_ = mtsTransform_->innerStateOnSurface(t, *(trackerHandle_.product()), theMagField.product());
    if (!innTSOS_.isValid()) return false;

    //at vertex
    // innermost state propagation to the beam spot position
    vtxTSOS_ 
      = TransverseImpactPointExtrapolator(*geomPropBw_).extrapolate(innTSOS_,GlobalPoint(bsPosition.x(),bsPosition.y(),bsPosition.z()));
    if (!vtxTSOS_.isValid()) vtxTSOS_=innTSOS_;

    //at seed
    outTSOS_ 
      = mtsTransform_->outerStateOnSurface(t, *(trackerHandle_.product()), theMagField.product());
    if (!outTSOS_.isValid()) return false;
    
    //    TrajectoryStateOnSurface seedTSOS 
    seedTSOS_ 
     = TransverseImpactPointExtrapolator(*geomPropFw_).extrapolate(outTSOS_,GlobalPoint(theClus.seed()->position().x(),theClus.seed()->position().y(),theClus.seed()->position().z()));
    if (!seedTSOS_.isValid()) seedTSOS_=outTSOS_;

    //at scl
   sclTSOS_ 
    = TransverseImpactPointExtrapolator(*geomPropFw_).extrapolate(innTSOS_,GlobalPoint(theClus.x(),theClus.y(),theClus.z()));
    if (!sclTSOS_.isValid()) sclTSOS_=outTSOS_;
    return true;
}

void GlobalGsfElectronAlgo::resolveElectrons(std::vector<reco::GsfElectron> & tempEle, reco::GsfElectronCollection & outEle) {

  typedef std::set<const reco::GsfElectron *> set_container;
  typedef std::vector<const reco::GsfElectron *> container;
  typedef container::const_iterator const_iterator;
  typedef set_container::const_iterator set_const_iterator;

//  container selected;
  set_container resolved_el;
  std::multimap< const reco::GsfElectron *,  const reco::GsfElectron *> ambigus_el;  
  set_container ambigus;

//  selected_.clear();
//  resolved_el_.clear();
//  ambigus_el_.clear();

  // fill map of ambiguous electrons
  for( std::vector<reco::GsfElectron>::const_iterator el1 = tempEle.begin();  el1 != tempEle.end(); el1++ ) {
    for( std::vector<reco::GsfElectron>::const_iterator el2 = el1+1;  el2 != tempEle.end(); el2++ ) {
      if((el1->caloEnergy() == el2->caloEnergy() && 
      	  el1->caloPosition() == el2->caloPosition()) || 
      	 (el1->gsfTrack()== el2->gsfTrack())) {
//	std::cout<<"ambigus : "<<el1->eta()<<" "<<el2->eta()
//	         <<" "<<el1->caloEnergy()<<" "<<el2->caloEnergy()
//	         <<" "<<el1->gsfTrack()->momentum().rho()<<" "<<el2->gsfTrack()->momentum().rho()
//	         <<" "<<el1->trackMomentumAtVtx().rho()<<" "<<el2->trackMomentumAtVtx().rho()
//	         <<" "<<el1->p4().t()<<" "<<el2->p4().t()
//	         <<" "<<el1->eSuperClusterOverP()<<" "<<el2->eSuperClusterOverP()
//		 <<std::endl;
      	ambigus_el.insert(pair<const GsfElectron *,const GsfElectron *>(&(*el1),&(*el2)));
	ambigus.insert(&(*el1));
	ambigus.insert(&(*el2));
      }
    }
    set<const reco::GsfElectron *>::iterator it = ambigus.find(& * el1);
    //    if (it == ambigus.end()) resolved_el_.push_back(& * el1);
    if (it == ambigus.end()) {
      resolved_el.insert(&(*el1));
      outEle.push_back(*el1);
    }
  }

  // resolve ambiguities
  for (map< const reco::GsfElectron *,  const reco::GsfElectron *>::iterator it2= ambigus_el.begin(); it2!=ambigus_el.end();it2++){
    //cout<<"ambigus = "<<it2->first->eta()<<" "<<it2->second->eta()<<endl;
    if (fabs(it2->first->eSuperClusterOverP()-1)<= fabs(it2->second->eSuperClusterOverP()-1)){
      set_const_iterator it= resolved_el.find(it2->first);
      if (it == resolved_el.end()) {
	resolved_el.insert(it2->first);
	outEle.push_back(*it2->first);
      }
    }
    else  {
      set_const_iterator it= resolved_el.find(it2->second);
      if (it == resolved_el.end()){
	resolved_el.insert(it2->second);
	outEle.push_back(*it2->second);

      }
    }      
  }    

}

void GlobalGsfElectronAlgo::process(edm::Handle<GsfTrackCollection> tracksH,
                            edm::Handle<reco::SuperClusterCollection> superClustersBarrelH,
                            edm::Handle<reco::SuperClusterCollection> superClustersEndcapH,
			    const math::XYZPoint &bsPosition,
                            GsfElectronCollection & outEle) {
  
  
  if (tracksH->size() == 0) {
    //std::cout << "Electron lost: no track found. " << std::endl;
  } else {
    //std::cout << "Number of tracks: " << tracksH->size() << std::endl;
  }
  
  //std::cout << "SuperCluster: " << superClustersBarrelH->size() << "  " << 
  //  superClustersEndcapH->size() << std::endl;	

  reco::SuperClusterRefVector superClusters;

  for(int z=0; z<2; ++z) {

    superClusters.clear();
    if (z == 0) {
      for(reco::SuperClusterCollection::size_type i= 0; i<superClustersBarrelH->size(); ++i){
        reco::SuperClusterRef cluster(superClustersBarrelH, i);
        superClusters.push_back(cluster);
      }
      //std::cout << superClustersBarrelH->size() << std::endl;
    }
    
    if (z == 1) {
      for(reco::SuperClusterCollection::size_type i= 0; i<superClustersEndcapH->size(); ++i){
        reco::SuperClusterRef cluster(superClustersEndcapH, i);
        superClusters.push_back(cluster);
      }
      //std::cout << superClustersEndcapH->size() << std::endl;
    }
    
    //================= loop over SuperClusters ===============

    for(unsigned int i=0; i< superClusters.size(); ++i) {

      //std::cout << "Start matching " << std::endl;	
      reco::SuperClusterRef scRef = superClusters[i];
      reco::SuperCluster theClus = (*scRef);
      reco::GsfTrackRef trackRef = superClusterMatching(scRef, tracksH);
      
      if(trackRef.isNull()) {
        //std::cout << "Electron lost: no supercluster match found: " << tracksH->size() << std::endl;
        continue;
      }
      std::vector<DetId> vecId=theClus.seed()->getHitsByDetId();
      subdet_ =vecId[0].subdetId();  

    
      // calculate Trajectory StatesOnSurface....
      if (!calculateTSOS((*trackRef),theClus,bsPosition)) continue;

      vtxMom_=computeMode(vtxTSOS_);
      sclPos_=sclTSOS_.globalPosition();
      if (preSelection(theClus)) {
	// interface to be improved...
	createElectron(scRef,trackRef,outEle);
        //LogInfo("")<<"Constructed new electron with energy  "<< (*sclAss)[seed]->energy();
      }
    }  
  }
}

const reco::GsfTrackRef
GlobalGsfElectronAlgo::superClusterMatching(reco::SuperClusterRef sc, edm::Handle<reco::GsfTrackCollection> tracks) {

  double minDr = 0.5;
  double minDeop = 10.;
  //reco::SuperClusterRef theClus = edm::Ref<SuperClusterCollection>();
  reco::GsfTrackRef theTrack = edm::Ref<reco::GsfTrackCollection>();


  for(reco::GsfTrackCollection::size_type i=0; i<tracks->size(); ++i){
    reco::GsfTrackRef track(tracks, i);
    math::XYZVector trackGlobalDir(track->momentum());   
    math::XYZVector clusterGlobalDir(sc->x() - track->vx(), sc->y() - track->vy(), sc->z() - track->vz());
    //math::XYZVector clusterGlobalPos(sc->x(), sc->y(), sc->z());
    
    double clusEt = sc->energy()*sin(clusterGlobalDir.theta());
    double clusEstimatedCurvature = clusEt/0.3/4*100;  //4 tesla (temporary solution)
    double DphiBending = sc->position().rho()/2./clusEstimatedCurvature; //ecal radius


    double tmpDr = ROOT::Math::VectorUtil::DeltaR(clusterGlobalDir, trackGlobalDir);
    if ( !(tmpDr < minDr) ) continue;

    TrajectoryStateOnSurface innTSOS = mtsTransform_->innerStateOnSurface(*track, *(trackerHandle_.product()), theMagField.product());
    GlobalVector innMom=computeMode(innTSOS);

    TrajectoryStateOnSurface outTSOS = mtsTransform_->outerStateOnSurface(*track, *(trackerHandle_.product()), theMagField.product());
    if (!outTSOS.isValid())   continue;

    TrajectoryStateOnSurface seedTSOS = TransverseImpactPointExtrapolator(*geomPropFw_).extrapolate(outTSOS,GlobalPoint(sc->seed()->position().x(),sc->seed()->position().y(),sc->seed()->position().z()));
    if (!seedTSOS.isValid()) seedTSOS=outTSOS;

    GlobalVector seedMom=computeMode(seedTSOS);

    double eOverPin  = sc->energy()/innMom.mag();
    //    double eOverPout = sc->seed()->energy()/seedMom.mag();
 
    double Deta = fabs(clusterGlobalDir.eta() - trackGlobalDir.eta());
    double dPhi = fabs(acos(cos(clusterGlobalDir.phi() - trackGlobalDir.phi())));
    float dPhi1 = fabs(dPhi - DphiBending);
    float dPhi2 = fabs(dPhi + DphiBending);

    //    if( !(eOverPout>0.5) ) continue;
    if( !(eOverPin<5) )  continue;
    if( !(min(dPhi1,dPhi2) < 0.1) )  continue;
    if( !(Deta < 0.02) ) continue;

    //    cout << " in matchbox, dphi, deta: " << Dphi << " , " << Deta << endl;
    //    cout << " in matchbox, E/Pin, out: " << eOverPin << " , " << eOverPout << endl;

    if( fabs(eOverPin-1.) < minDeop){
      minDeop = fabs(eOverPin-1.) ;
      theTrack = track;
    }
  }

  //cout << " in matchbox, minD(eop): " << minDeop << endl;
  //std::cout << "returning null ref" << std::endl;
  return theTrack;
}