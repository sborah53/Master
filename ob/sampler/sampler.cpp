#include "sampler.h"
#include <iostream>

Sampler::Sampler(int nSamples, int nCycles, Hamiltonian &hamiltonian,
                 NeuralQuantumState &nqs, Optimizer &optimizer,
                 std::string filename, std::string blockFilename, int seed) :
    m_hamiltonian(hamiltonian), m_nqs(nqs), m_optimizer(optimizer) {

    m_nSamples = nSamples;
    m_nCycles = nCycles;
    m_gibbsfactor = 1.0;      // Changed to 0.5 if called in the Gibbs subclass constructor.
    m_outfile.open(filename);
    m_blockOutfile.open(blockFilename);
    m_randomEngine = std::mt19937_64(seed);
}


void Sampler::runOptimizationSampling() {
    int nPar = m_nqs.m_nx + m_nqs.m_nh + m_nqs.m_nx*m_nqs.m_nh;
    double Eloc_temp;
    double variance;
    double acceptedRatio;
    // Wf derived wrt rbm parameters, to be added up for each sampling
    Eigen::VectorXd derPsi;
    Eigen::VectorXd derPsi_temp;
    derPsi.resize(nPar);
    derPsi_temp.resize(nPar);
    // Local energy times wf derived wrt rbm parameters, to be added up for each sampling
    Eigen::VectorXd EderPsi;
    EderPsi.resize(nPar);
    // The gradient
    Eigen::VectorXd grad;
    grad.resize(nPar);


    // Variables to store summations during sampling
    double Eloc;
    double Eloc2;
    double effectiveSamplings;
    bool accepted = false;
    int acceptcount;

    // Optimization iterations
    for (int cycles=0; cycles<m_nCycles; cycles++) {
        Eloc = 0;
        Eloc2 = 0;
        effectiveSamplings = 0;
        acceptcount = 0;
        derPsi.setZero();
        EderPsi.setZero();

        // Compute Psi and relevant components according to the new weights and biases
        Eigen::VectorXd Q = m_nqs.m_b + (1.0/m_nqs.m_sig2)*(m_nqs.m_x.transpose()*m_nqs.m_w).transpose();
        m_nqs.m_psi = m_nqs.computePsi(m_nqs.m_x, Q);
        m_nqs.updatePsiComponents(Q);

        // Samples
        for (int samples=0; samples<m_nSamples; samples++) {
            samplePositions(accepted);
            if (samples > 0.1*m_nSamples) {
                Eloc_temp = m_hamiltonian.computeLocalEnergy(m_nqs, m_gibbsfactor);
                derPsi_temp = m_hamiltonian.computeLocalEnergyGradientComponent(m_nqs, m_gibbsfactor);
                //std::cout << Eloc_temp << std::endl;
                // Add up values for expectation values
                Eloc += Eloc_temp;
                Eloc2 += Eloc_temp*Eloc_temp;
                effectiveSamplings++;
                derPsi += derPsi_temp;
                EderPsi += Eloc_temp*derPsi_temp;
                if (accepted) acceptcount++;

                // Write the energies for blocking - interested in the final optimization cycle only
                if (cycles==m_nCycles-1) {
                    m_blockOutfile << Eloc_temp << "\n";
                }
            }
        }

        // Compute expectation values
        Eloc = Eloc/effectiveSamplings;
        Eloc2 = Eloc2/effectiveSamplings;
        derPsi = derPsi/effectiveSamplings;
        EderPsi = EderPsi/effectiveSamplings;

        // Other quantities of interest
        variance = Eloc2 - Eloc*Eloc;
        acceptedRatio = acceptcount/(double)effectiveSamplings;

        /*
        // Write parameters of last cycle to file
        if (cycles==m_nCycles-1) {
            std::ofstream paramfile;
            paramfile.open("/Users/Vilde/Documents/masters/NQS_paper/tryHOrbm/Theory/parameters.txt");

            for (int i=0; i<m_nqs.m_nx; i++) {
                paramfile << m_nqs.m_a(i) << " ";
            }
            paramfile << '\n';
            for (int j=0; j<m_nqs.m_nh; j++) {
                paramfile << m_nqs.m_b(j) << " ";
            }
            paramfile << '\n';
            int k = m_nqs.m_nx + m_nqs.m_nh;
            for (int i=0; i<m_nqs.m_nx; i++) {
                for (int j=0; j<m_nqs.m_nh; j++) {
                    paramfile << m_nqs.m_w(i,j) << " ";
                    k++;
                }
            }
            paramfile.close();
        }
        */

        // Compute gradient
        grad = 2*(EderPsi - Eloc*derPsi);

        // Update weights
        m_optimizer.optimizeWeights(m_nqs, grad, cycles);

        // Terminal output
        std::cout << cycles << "   " << Eloc << "   " << variance << "   " //<< a(0) << "   "
             << acceptedRatio << //"  " << effectiveSamplings << " " << acceptcount <<
             //<< a(1) << " "
             //<< b(0) << " " << b(1) << " " << b(2) << " " << b(3) << " "
             //<< w(0,0) << " " << w(0,1) << " " << w(0,2) << " " << w(0,3) << " "
             //<< w(1,0) << " " << w(1,1) << " " << w(1,2) << " " << w(1,3) <<
               std::endl;

        // File output
        m_outfile << cycles << "   " << Eloc << "   " << variance << "   "
                << acceptedRatio << "\n";





    }
    m_outfile.close();
}
