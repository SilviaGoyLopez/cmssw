// HepMC Headers
#include "HepMC/PythiaWrapper6_4.h"

// Pythia6 framework integration service Headers
#include "GeneratorInterface/Pythia6Interface/interface/Pythia6Service.h"

// FAMOS Headers
#include "FastSimulation/ParticlePropagator/interface/ParticlePropagator.h"
#include "FastSimulation/ParticleDecay/interface/PythiaDecays.h"
#include "FastSimulation/ParticleDecay/interface/Pythia6jets.h"
#include "FastSimulation/ParticleDecay/interface/RandomP8.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
// move it here from the header
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"

// Needed for Pythia6 
#define PYTHIA6PYDECY pythia6pydecy_

extern "C" {
  void PYTHIA6PYDECY(int *ip);
}

PythiaDecays::PythiaDecays(std::string program)
{
  program_=program;
  if (program_ == "pythia6") {
    //// Pythia6:
    pyjets = new Pythia6jets();
    pyservice = new gen::Pythia6Service();
    // The PYTHIA decay tables will be initialized later 
  } else if (program_ == "pythia8") {
    //// Pythia8:
    // --> no need pythia.reset(new Pythia8::Pythia);
    decayer.reset(new Pythia8::Pythia);
    RandomP8* RP8 = new RandomP8();
    // get rndm engine by the framework
    edm::Service<edm::RandomNumberGenerator> rng;
    if(!rng.isAvailable()) {
    throw cms::Exception("Configuration")
       << "The RandomNumberProducer module requires the RandomNumberGeneratorService\n"
          "which appears to be absent.  Please add that service to your configuration\n"
          "or remove the modules that require it." << std::endl;
    }
// The Service has already instantiated an engine.  Make contact with it.
    randomEngine = &(rng->getEngine());    
    // ---> no need pythia->setRndmEnginePtr(RP8);
    decayer->setRndmEnginePtr(RP8);
    // init decayer
    decayer->readString("ProcessLevel:all = off"); // The trick!
    decayer->readString("ParticleDecays:sophisticatedTau = 0"); // safer option with old-style tau decays
    decayer->readString("ParticleDecays:limitTau = off");
    decayer->readString("ParticleDecays:limitTau0 = off");
    decayer->readString("ParticleDecays:tauMax = 999999");
    decayer->readString("ParticleDecays:tau0Max = 999999");
    decayer->init();    

    // print out settings
    //decayer->settings.listChanged();
    //decayer->settings.listAll();
    //decayer->particleData.listChanged(); 
    //decayer->particleData.listAll();

  } else {
    std::cout << "WARNING: you are requesting an option which is not available in PythiaDecays::PythiaDecays " << std::endl;
  }

}

PythiaDecays::~PythiaDecays() {
  if (program_ == "pythia6") {
    delete pyjets;
    delete pyservice;
  }
}

const DaughterParticleList&
PythiaDecays::particleDaughtersPy8(ParticlePropagator& particle)
{

  theList.clear();

  /*
  std::cout << "###" << std::endl;
  std::cout << "particle.PDGname() == " << particle.PDGname() << std::endl; 
  std::cout << "particle.status() == " << particle.status() << std::endl;
  std::cout << "particle.pid() == " << particle.pid() << std::endl;
  std::cout << "particle.momentum().x() == " << particle.momentum().x() << std::endl;
  std::cout << "particle.Px() == " << particle.Px() << std::endl;
  std::cout << "particle.X() == " << particle.X() << std::endl;
  std::cout << "particle.mass() == " << particle.mass() << std::endl;
  std::cout << "particle.PDGmass() == " << particle.PDGmass() << std::endl;
  std::cout << "particle.PDGcTau() == " << particle.PDGcTau() << std::endl;
  std::cout << "(decayer->particleData).tau0( particle.pid() ) == " << (decayer->particleData).tau0( particle.pid() ) << std::endl;
  */

  // inspired by method Pythia8Hadronizer::residualDecay() in GeneratorInterface/Pythia8Interface/src/Pythia8Hadronizer.cc
  decayer->event.reset();
  Pythia8::Particle py8part(  particle.pid(), 93, 0, 0, 0, 0, 0, 0,
		     particle.momentum().x(), // note: momentum().x() and Px() are the same
		     particle.momentum().y(),
		     particle.momentum().z(),
		     particle.momentum().t(),
		     particle.mass() );
  py8part.vProd( particle.X(), particle.Y(), 
		 particle.Z(), particle.T() );
  py8part.tau( (decayer->particleData).tau0( particle.pid() ) );
  decayer->event.append( py8part );

  int nentries = decayer->event.size();
  if ( !decayer->event[nentries-1].mayDecay() ) return theList;
  
  // print out event content before decays
  //
  //  decayer->event.list();
  
  decayer->next();
  
  // print out event content after decays
  //
  //decayer->event.list();
  
  int nentries1 = decayer->event.size();
  if ( nentries1 <= nentries ) return theList; //same number of particles, no decays...

  
  // now we need to fill up the list of daughters
  // Note: remember that the Py8 event record always contains "system particle", 
  //       so the Py8 record is always +1 longer than "we want";
  //       To offset for it, we need to take the final number of particles and
  //       reduce it by 2 - 1 for "system particle", and 1 for the one that decays
  
  theList.clear();
  theList.resize(nentries1-nentries,RawParticle());

  for ( int ipart=nentries; ipart<nentries1; ipart++ )
    {
      Pythia8::Particle& py8daughter = decayer->event[ipart]; 
      theList[ipart-nentries].SetXYZT( py8daughter.px(), py8daughter.py(), py8daughter.pz(), py8daughter.e() );
      theList[ipart-nentries].setVertex( py8daughter.xProd(),
					   py8daughter.yProd(),
					   py8daughter.zProd(),
					   py8daughter.tProd() );
      theList[ipart-nentries].setID( py8daughter.id() );
      theList[ipart-nentries].setMass( py8daughter.m() );
    }

  return theList;
}

const DaughterParticleList&
PythiaDecays::particleDaughtersPy6(ParticlePropagator& particle)
{
  gen::Pythia6Service::InstanceWrapper guard(pyservice); // grab Py6 context

  //  Pythia6jets pyjets;
  int ip;

  pyjets->k(1,1) = 1;
  pyjets->k(1,2) = particle.pid();
  pyjets->p(1,1) = particle.Px();
  pyjets->p(1,2) = particle.Py();
  pyjets->p(1,3) = particle.Pz();
  pyjets->p(1,4) = std::max(particle.mass(),particle.e());
  pyjets->p(1,5) = particle.mass();
  pyjets->v(1,1) = particle.X();
  pyjets->v(1,2) = particle.Y();
  pyjets->v(1,3) = particle.Z();
  pyjets->v(1,4) = particle.T();
  pyjets->n() = 1;
  
  ip = 1;
  PYTHIA6PYDECY(&ip);

  // Fill the list of daughters
  theList.clear();
  if ( pyjets->n()==1 ) return theList; 

  theList.resize(pyjets->n()-1,RawParticle());

  for (int i=2;i<=pyjets->n();++i) {
    
    theList[i-2].SetXYZT(pyjets->p(i,1),pyjets->p(i,2),
			 pyjets->p(i,3),pyjets->p(i,4)); 
    theList[i-2].setVertex(pyjets->v(i,1),pyjets->v(i,2),
			   pyjets->v(i,3),pyjets->v(i,4));
    theList[i-2].setID(pyjets->k(i,2));
    theList[i-2].setMass(pyjets->p(i,5));

  }

  return theList;
  
}
