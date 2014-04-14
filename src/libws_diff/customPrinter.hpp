/*
 * customPrinter.h
 *
 *  Created on: 24 janv. 2014
 *      Author: cvallee
 */
#pragma once

#include <iostream>
#include <vector>
#include "dtl.hpp"
#include "variables.hpp"
#include "functors.hpp"

namespace dtl {

    /**
     * allow same action charactere to be on the same line
     * concatenate the same sesElem action element on the same line
     */
    template <typename sesElem, typename stream = ostream >
    class customPrinter : public Printer < sesElem, stream >
    {
    	mutable int prevType;
    public :

    	customPrinter  ()            : Printer < sesElem, stream > (),prevType(0)   {}
    	customPrinter  (stream& out) : Printer < sesElem, stream > (out),prevType(0) {}

    	~customPrinter () {}
        void operator() (const sesElem& se) const {
        	if (prevType != se.second.type){
        		prevType = se.second.type;
				switch (se.second.type) {
				case SES_ADD:
					this->out_ << endl << SES_MARK_ADD    << se.first;
					break;
				case SES_DELETE:
					this->out_ << endl << SES_MARK_DELETE << se.first;
					break;
				case SES_COMMON:
					this->out_ << endl << SES_MARK_COMMON << se.first;
					break;
				}
        	}else{
        		this->out_ << se.first;
        	}
        }
    };

    /**
     * unified format element printer class template
     */
    template <typename sesElem, typename stream = ostream >
    class customHunkPrinter
    {
    public :
    	customHunkPrinter  ()            : out_(cout) {}
    	customHunkPrinter  (stream& out) : out_(out)  {}
        ~customHunkPrinter () {}
        void operator() (const uniHunk< sesElem >& hunk) const {
            out_ << "@@"
                 << " -"  << hunk.a << "," << hunk.b
                 << " +"  << hunk.c << "," << hunk.d
                 << " @@" << endl;
            customPrinter< sesElem, stream > myPrinter=customPrinter< sesElem, stream >(out_);
            for_each(hunk.common[0].begin(), hunk.common[0].end(), myPrinter);
            for_each(hunk.change.begin(),    hunk.change.end(), myPrinter);
            for_each(hunk.common[1].begin(), hunk.common[1].end(), myPrinter);
        }
    private :
        stream& out_;
    };
}
