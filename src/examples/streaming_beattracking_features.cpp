/*
 * Copyright (C) 2006-2016  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include <iostream>
#include <essentia/algorithmfactory.h>
#include <essentia/streaming/algorithms/poolstorage.h>
#include <essentia/scheduler/network.h>
#include "credit_libav.h"

using namespace std;
using namespace essentia;
using namespace essentia::streaming;
using namespace essentia::scheduler;

int main(int argc, char* argv[]) {

  if (argc != 3) {
    cout << "Error: incorrect number of arguments." << endl;
    cout << "Usage: " << argv[0] << " audio_input yaml_output" << endl;
    creditLibAV();
    exit(1);
  }

  string audioFilename = argv[1];
  string outputFilename = argv[2];

  // register the algorithms in the factory(ies)
  essentia::init();

  Pool pool;

  /////// PARAMS //////////////
  Real sampleRate = 44100.0;
  int frameSize = 2048;
  int hopSize = 1024;

  // we want to compute the MFCC of a file: we need the create the following:
  // audioloader -> framecutter -> windowing -> FFT -> MFCC -> PoolStorage
  // we also need a DevNull which is able to gobble data without doing anything
  // with it (required otherwise a buffer would be filled and blocking)

  AlgorithmFactory& factory = streaming::AlgorithmFactory::instance();

  Algorithm* audio = factory.create("MonoLoader",
                                    "filename", audioFilename,
                                    "sampleRate", sampleRate);

  Algorithm* fc1   = factory.create("FrameCutter",
                                    "frameSize", 1024,
                                    "hopSize", hopSize,
                                    "silentFrames", "noise");

  Algorithm* fc2   = factory.create("FrameCutter",
                                    "frameSize", 2048,
                                    "hopSize", hopSize,
                                    "silentFrames", "noise");

  Algorithm* fc3   = factory.create("FrameCutter",
                                    "frameSize", 4096,
                                    "hopSize", hopSize,
                                    "silentFrames", "noise");

  Algorithm* w1    = factory.create("Windowing",
                                    "type", "Hann");

  Algorithm* w2    = factory.create("Windowing",
                                    "type", "Hann");

  Algorithm* w3    = factory.create("Windowing",
                                    "type", "Hann");

  Algorithm* spec1  = factory.create("SpectrumCQ",
                                     "minFrequency", 30,
                                     "maxFrequency", 17000,
                                     "binsPerOctave", 3,
                                     );
  Algorithm* spec2  = factory.create("SpectrumCQ",
                                     "minFrequency", 30,
                                     "maxFrequency", 17000,
                                     "binsPerOctave", 6,
  Algorithm* spec3  = factory.create("SpectrumCQ",
                                     "minFrequency", 30,
                                     "maxFrequency", 17000,
                                     "binsPerOctave", 12,
  
  // Algorithm* mfcc1  = factory.create("MFCC");
  // Algorithm* mfcc2  = factory.create("MFCC");
  // Algorithm* mfcc3  = factory.create("MFCC");


  /////////// CONNECTING THE ALGORITHMS ////////////////
  cout << "-------- connecting algos --------" << endl;

  // Audio -> FrameCutter
  audio->output("audio")     >>  fc1->input("signal");
  audio->output("audio")     >>  fc2->input("signal");
  audio->output("audio")     >>  fc3->input("signal");

  // FrameCutter -> Windowing -> Spectrum
  fc1->output("frame")       >>  w1->input("frame");
  fc2->output("frame")       >>  w2->input("frame");
  fc3->output("frame")       >>  w3->input("frame");
  
  w1->output("frame")        >>  spec1->input("frame");
  w2->output("frame")        >>  spec2->input("frame");
  w3->output("frame")        >>  spec3->input("frame");

  vector<Real> stacked_spectrum

  mfcc1->output("bands")     >>  NOWHERE;                          // we don't want the mel bands
  mfcc1->output("mfcc")      >>  PC(pool, "lowlevel.mfcc"); // store only the mfcc coeffs

  mfcc2->output("bands")     >>  NOWHERE;                          // we don't want the mel bands
  mfcc2->output("mfcc")      >>  PC(pool, "lowlevel.mfcc"); // store only the mfcc coeffs

  mfcc3->output("bands")     >>  NOWHERE;                          // we don't want the mel bands
  mfcc3->output("mfcc")      >>  PC(pool, "lowlevel.mfcc"); // store only the mfcc coeffs

  // Note: PC is a #define for PoolConnector


  /////////// STARTING THE ALGORITHMS //////////////////
  cout << "-------- start processing " << audioFilename << " --------" << endl;

  // create a network with our algorithms...
  Network n(audio);
  // ...and run it, easy as that!
  n.run();

  // aggregate the results
  Pool aggrPool; // the pool with the aggregated MFCC values
  const char* stats[] = { "mean", "var", "min", "max", "cov", "icov" };

  standard::Algorithm* aggr = standard::AlgorithmFactory::create("PoolAggregator",
                                                                 "defaultStats", arrayToVector<string>(stats));

  aggr->input("input").set(pool);
  aggr->output("output").set(aggrPool);
  aggr->compute();

  aggrPool.merge("lowlevel.mfcc.frames", pool.value<vector<vector<Real> > >("lowlevel.mfcc"));

  // write results to file
  cout << "-------- writing results to file " << outputFilename << " --------" << endl;

  standard::Algorithm* output = standard::AlgorithmFactory::create("YamlOutput",
                                                                   "filename", outputFilename);
  output->input("pool").set(aggrPool);
  output->compute();

  // NB: we could just wait for the network to go out of scope, but then this would happen
  //     after the call to essentia::shutdown() where the FFTW structs would already have
  //     been freed, so let's just delete everything explicitly now
  n.clear();

  delete aggr;
  delete output;
  essentia::shutdown();

  return 0;
}
