/**
 * isotopedistribution.cpp
 *
 * Author: Anton Pervukhin <Anton.Pervukhin@CeBiTec.Uni-Bielefeld.DE>
 *
 */

#include <functional>
#include <numeric>
#include <iostream>
#include <cmath>
#include <ims/isotopedistribution.h>

namespace ims {

IsotopeDistribution::size_type IsotopeDistribution::SIZE;

IsotopeDistribution::abundance_type IsotopeDistribution::ABUNDANCES_SUM_ERROR;

/**
 * Constructor with single isotope. It sets isotopes consist of one entry 
 * with given mass and 100% abundance.
 */
IsotopeDistribution::IsotopeDistribution(mass_type mass): nominalMass(0) {
	peaks.push_back(peaks_container::value_type(mass, 1.0));
}


IsotopeDistribution& IsotopeDistribution::operator =(
								const IsotopeDistribution& distribution) {
	if (this != &distribution) {
		peaks = distribution.peaks;
		nominalMass = distribution.nominalMass;
	}
	return *this;
}


bool IsotopeDistribution::operator ==(
							const IsotopeDistribution& distribution) const {
	return ( this == &distribution ||
			(peaks == distribution.peaks &&
			 nominalMass == distribution.nominalMass));
}


bool IsotopeDistribution::operator !=(
							const IsotopeDistribution& distribution) const {
	return !this->operator==(distribution);
}


IsotopeDistribution& IsotopeDistribution::operator *=(
					const IsotopeDistribution& distribution) {

	if (distribution.empty()) {
		return *this;
	}
	if (this->empty()) {
		return operator =(distribution);
	}
	// creates a temporary destination container to store peaks 
	// (abundances and masses)
	peaks_container dest(SIZE);
	// checks if the size of abundances and masses containers coincides with 
	// the static variable SIZE (meant to be set by client for distribution)
	setMinimumSize();
	// creates a non-const equivalent of a const parameter - it's needed to 
	// get non-const iterators out of it
	IsotopeDistribution& non_const_distribution = 
							const_cast<IsotopeDistribution&>(distribution);
	non_const_distribution.setMinimumSize();

	// sets up different iterators for an efficient folding:
	// it2_dest - iterator on a destination container,
	// it2_begin, it2_end - iterators on begin and end of the second 
	//							source container (function parameter)
	// it1 - iterator on the first source container
	// it2 - iterator on the second source container
	peaks_iterator it2_begin = non_const_distribution.peaks.begin();
	peaks_iterator it1, it2,
			it_dest = dest.begin(),
			it2_end = it2_begin;
	abundance_type abundances_sum, masses_mult_abundances_sum;

	for (; it_dest != dest.end(); ++it_dest, ++it2_end) {

		abundances_sum = 0;
		masses_mult_abundances_sum = 0;
		it1 = peaks.begin();
		it2 = it2_end;

		for (; it2 != it2_begin; ++it1, --it2) {			
			abundances_sum += it1->abundance * it2->abundance;
			masses_mult_abundances_sum += 
					it1->abundance * it2->abundance * (it1->mass + it2->mass);
		}
		// adds last element
		abundances_sum += it1->abundance * it2->abundance;
		masses_mult_abundances_sum += 
					it1->abundance * it2->abundance * (it1->mass + it2->mass);

		// assigns results to containers through iterators
		it_dest->abundance = abundances_sum;
		it_dest->mass = (abundances_sum != 0) ?
							masses_mult_abundances_sum / abundances_sum : 0;
	}

	nominalMass += distribution.nominalMass;
	
	peaks.swap(dest);
	
	this->normalize();

	return *this;
}

/**
 * Folds the distribution with itself @c power times. Implements
 * Russian Multiplication Scheme by this reducing the number of 
 * folding operations. For the sake of performance folding is 
 * implemented iteratively, not recursively.
 * 
 * @return The distribution folded with itself @c power times.
 */
IsotopeDistribution& IsotopeDistribution::operator *=(unsigned int power) {
	if (power <= 1) {
		return *this;
	}
	
	// folding proceeds a following:
	// - first, binary representation of power is calculated, i.e. 
	// power = 138 -----> binary representation = [0, 1, 0, 1, 0, 0, 0, 1]
	// - then, one loops through array every time folding the copy
	// of this distribution with itself into lets say this_power_two_index distribution. 
	// Additionally, if the current index is equal to 1, then result distribution
	// is folded with the current this_power_two_index distribution.
	// At the end, the result distribution is outputted.
	
	// calculates binary representation of power
	std::vector<unsigned int> binary;
	while (power > 0) {
		binary.push_back(power % 2);
		power >>= 1;
	}
	
	// initializes distribution which will folded iteratively upto each entry
	IsotopeDistribution this_power_two_index(*this);
	
	// initializes result distribution where foldings will be collected
	IsotopeDistribution result;
	
	// starts folding based on binary representation
	if (binary[0]) {
		result = this_power_two_index;
	}
		
	std::vector<unsigned int>::size_type index = 0;
	while (++index < binary.size()) {
		// folds distribution with itself iteratively
		this_power_two_index *= this_power_two_index;
		
		if (binary[index]) {
			// collects distribution in the result
			result *= this_power_two_index;
		}
	}
		
	return operator=(result);
		
}


IsotopeDistribution::mass_type IsotopeDistribution::getAverageMass() const {
	mass_type average_mass = 0.0;
	for (size_type i = 0; i < peaks.size(); ++i) {
		average_mass += this->getMass(i) * this->getAbundance(i);
	}
	return average_mass;
}


IsotopeDistribution::abundances_container IsotopeDistribution::
getAbundances() const {
	abundances_container _abundances;
	for (size_type i = 0; i < size(); ++i) {
		_abundances.push_back(this->getAbundance(i));
	}
	return _abundances;
}


IsotopeDistribution::masses_container IsotopeDistribution::getMasses() const {
	masses_container _masses;
	for (size_type i = 0; i < size(); ++i) {
		_masses.push_back(this->getMass(i));
	}
	return _masses;
}


void IsotopeDistribution::normalize() {
	abundance_type sum = 0.0;
	for (const_peaks_iterator cit = peaks.begin(); cit < peaks.end(); ++cit) {
		sum += cit->abundance;
	}
	if (sum > 0 && std::fabs(sum - 1) > ABUNDANCES_SUM_ERROR) {
		abundance_type scale = 1/sum;
		for (peaks_iterator it = peaks.begin(); it < peaks.end(); ++it) {
			it->abundance *= scale;
		}
	}
}


void IsotopeDistribution::setMinimumSize() {
	if (peaks.size() < SIZE) {
		peaks.resize(SIZE);
	}
}


std::ostream& operator <<(std::ostream& os, 
							const IsotopeDistribution& distribution) {
	for (IsotopeDistribution::size_type i = 0; i < distribution.size(); ++i) {
		os 	<< distribution.getMass(i) << ' ' 
			<< distribution.getAbundance(i) 	<< '\n';
	}
	return os;
}

} // namespace ims
