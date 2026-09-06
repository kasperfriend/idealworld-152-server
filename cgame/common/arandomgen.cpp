/*
	arandomgen.cpp - ARandomGen, the uniform / gaussian random generator of
	the cmlib (abase) support library.

	The class layout in cgame/include/arandomgen.h (double u[97], c, cd, cm
	plus the two indices and the "is seeded" flag) is the Numerical Recipes
	port of the Marsaglia multiply-with-subtract generator ("ran1"), which is
	what the engine expects: RandomUniform() must stay inside (0,1) because
	RandInt/RandFloat scale it directly and dozens of drop tables compare the
	result against a probability.

	abase::__global_RandomGen is the shared generator that the inline
	abase::Rand*() helpers use; every one of them takes
	abase::__global_RandomLock (a spinlock int) around the call, so the
	generator itself does not lock.
*/

#include "arandomgen.h"

#include <math.h>
#include <time.h>
#include <unistd.h>

#define BFS_MULTIPLIER	537		/* NR1: k = (m[j]*537 + m[j+1]) % 4093 */
#define BFS_MODULO	4093

ARandomGen::ARandomGen()
	: c(0.0), cd(0.0), cm(0.0), m_i97(0), m_j97(0), m_bTest(0)
{
	/*
		Seeded from the clock and the pid instead of a fixed vector: a
		restarted gameserver should not replay the same loot and spawn
		sequence.  Callers that need reproducibility (the item / skill
		probability tests) call Init(seed) explicitly.
	*/
	unsigned int seed = (unsigned int)time(NULL) ^ ((unsigned int)getpid() << 11);
	Init(seed);
}

ARandomGen::ARandomGen(unsigned int seed)
	: c(0.0), cd(0.0), cm(0.0), m_i97(0), m_j97(0), m_bTest(0)
{
	Init(seed);
}

void ARandomGen::RandomInitialize(int ij, int kl)
{
	int i, j, k, m[4];
	double s, t;

	if (ij < 0) ij = -ij;
	if (kl < 0) kl = -kl;
	if (ij == 0 && kl == 0) kl = 1;

	m[0] = (ij / 177) % 177 + 2;
	m[1] = ij % 177 + 2;
	m[2] = (kl / 177) % 177 + 2;
	m[3] = kl % 177 + 2;

	for (i = 0; i < 97; i++)
	{
		s = 0.0;
		t = 0.5;
		for (j = 0; j < 4; j++)
		{
			k = ((m[j] * BFS_MULTIPLIER) + m[(j + 1) & 3]) % BFS_MODULO;
			m[j] = k;
			if (k & 1) s += t;
			t *= 0.5;
		}
		u[i] = s;
		if (s <= 0.0) u[i] = 0.5;
	}

	m_i97 = 1;
	m_j97 = 97;

	c  = 362436.0    / 16777216.0;
	cd = 7654321.0   / 16777216.0;
	cm = 16777213.0  / 16777216.0;

	m_bTest = 1;
}

double ARandomGen::RandomUniform()
{
	if (!m_bTest) RandomInitialize(1, 1);

	double uni = u[m_i97 - 1] - u[m_j97 - 1];
	if (uni <= 0.0) uni += 1.0;
	u[m_i97 - 1] = uni;

	if (--m_i97 == 0) m_i97 = 97;
	if (--m_j97 == 0) m_j97 = 97;

	c -= cd;
	if (c < 0.0) c += cm;

	uni -= c;
	if (uni < 0.0) uni += 1.0;
	if (uni >= 1.0) uni = 0.9999999;	/* keep the result in (0,1) */
	return uni;
}

/*
	Polar (Marsaglia) transform, without caching the second deviate: the
	class has no room for it and callers never rely on the pairing.
*/
double ARandomGen::RandomGaussian(double mean, double stddev)
{
	if (!m_bTest) RandomInitialize(1, 1);

	for (;;)
	{
		double v1 = 2.0 * RandomUniform() - 1.0;
		double v2 = 2.0 * RandomUniform() - 1.0;
		double w = v1 * v1 + v2 * v2;
		if (w >= 1.0 || w <= 0.0) continue;
		double fac = sqrt(-2.0 * log(w) / w);
		return mean + stddev * (v1 * fac);
	}
}

namespace abase
{

ARandomGen __global_RandomGen;
int __global_RandomLock = 0;

}
