
// Velocity Verlet integration with Langevin EoM

static void integrateLangevin(float dt, float temperature) 
{
	
	const float halfdtgamma = 0.5*GAMMA*dt;
	const float halfdtgammanorm = 1/(1+halfdtgamma);
	const float halfdtMass = 0.5*dt/MASS;
	const float cutsq = HARD_CUT*HARD_CUT;
	const float randFmult = sqrt(2*temperature*GAMMA*MASS/dt);
	
	const float sin1 = sin(PHI_1/180.*M_PI);
	const float cos1 = cos(PHI_1/180.*M_PI);
	const float sin2 = sin(PHI_2/180.*M_PI);
	const float cos2 = cos(PHI_2/180.*M_PI);

	intraE=0;
	interE=0;
	hardE=0;
	dihedralE=0;
	
	

	#pragma omp parallel 
	{
	
	int thread_num = omp_get_thread_num();
	int num_threads = omp_get_num_threads();
	int i,j,k;
	float del[3],norm,rsq;
	float inter;

	uint32_t myseed = seed[thread_num];
	
	
	#pragma omp for	schedule(static)
	for (i=0; i<2*N; i++) {

		// updating velocity by half a step
		// v = v(t+0.5dt)
		
		v[i][0] -= halfdtgamma*v[i][0];
		v[i][1] -= halfdtgamma*v[i][1];
		v[i][2] -= halfdtgamma*v[i][2];

		v[i][0] += halfdtMass*f[i][0];
		v[i][1] += halfdtMass*f[i][1];
		v[i][2] += halfdtMass*f[i][2];

		// updating position by a full step
		// x = x(t+dt)
		
		x[i][0]+= v[i][0]*dt ;
		x[i][1]+= v[i][1]*dt ;
		x[i][2]+= v[i][2]*dt ;

	}

	
	// calculate forces for the next timestep
	// f = f(t+dt)
	
/********************* BEGIN FORCE CALCULATION ***********************/

	// Initialize with random forces instead of zeros 
	
	#pragma omp for	schedule(static)
	for (int i=0; i<2*N; i++) {
		f[i][0]=ziggurat(thread_num)*randFmult;
		f[i][1]=ziggurat(thread_num)*randFmult;
		f[i][2]=ziggurat(thread_num)*randFmult;
		
//		f[i][0]=r4_nor(&myseed, kn,fn,wn)*randFmult;
//		f[i][1]=r4_nor(&myseed, kn,fn,wn)*randFmult;
//		f[i][2]=r4_nor(&myseed, kn,fn,wn)*randFmult;
		
//		printf("%d\t%d\t%f\n",thread_num,i,ziggurat(thread_num) );
//		printf("%d\t%d\t%f\n",thread_num,i,r4_nor(&myseed, kn,fn,wn) );
	}

//	seed[thread_num]=myseed;


	// Calculate forces from intra-strand bonds
	
	#pragma omp for	schedule(static) reduction(+:intraE)

	for (i=0; i<2*N-2; i++) {
		intraE += harmonic(i, i+2, K_BOND, INTRA_BOND_LENGTH);
	}

	// Calculate forces from inter-strand interaction
	
	#pragma omp for reduction(+:interE,dihedralE)
	for (i=0; i<N; i++) {
		j=2*i;
		k=j+1;
		
		// the ends are special, they are non breakable and have no dihedrals
		
		if (i == 0 || i == N-1) {
			harmonic(j, k, K_BOND, INTER_BOND_LENGTH);
		}

		// others have dihedrals if they are not already broken

		else {
			inter = harcos(j,k, K_BOND, INTER_BOND_LENGTH, INTER_BOND_CUT);

			if  ( inter != 0 ) { 
				interE += inter;
				isBound[i] = 1;
				dihedralE += dihedral (2*i-2, j, k, 2*i+3, K_DIHEDRAL, sin1, cos1, EPSILON_DIHEDRAL, INTER_BOND_LENGTH, INTER_BOND_CUT);
				dihedralE += dihedral (2*i+2, j, k, 2*i-1, K_DIHEDRAL, sin2, cos2, EPSILON_DIHEDRAL, INTER_BOND_LENGTH, INTER_BOND_CUT);
			}
			else 
				isBound[i]=0;
		}
	
		 
	}


	// Calculate forces form hard-core repulsion
	
	#pragma omp for	reduction(+:hardE)
	for (i=0; i<2*N; i++) for (k=1; k<neigh[i][0]+1;k++) {
//		printf("%d ", neigh[i][0]);
		j=neigh[i][k];
		hardE += hardcore(i, j, K_BOND, HARD_CUT, cutsq);

	}

/****************** END OF FORCE CALCULATION **************************/
	
	
	// final update on velocity 
	// v = v(t+dt)
	#pragma omp for schedule(static)	
	for (i=0; i<2*N; i++)  { 

			v[i][0] += f[i][0]*halfdtMass ;
			v[i][1] += f[i][1]*halfdtMass ;
			v[i][2] += f[i][2]*halfdtMass ;
			
			v[i][0] *=halfdtgammanorm;
                        v[i][1] *=halfdtgammanorm;
	                v[i][2] *=halfdtgammanorm;
	}
	}
}
