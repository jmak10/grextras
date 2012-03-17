/*
 * Copyright 2011 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <gnuradio/extras/multiply_const.h>
#include <gr_io_signature.h>
#include <stdexcept>
#include <complex>
#include <volk/volk.h>
#include <boost/bind.hpp>

using namespace gnuradio::extras;

typedef boost::function<int(int, gr_vector_const_void_star, gr_vector_void_star, const void *)> volk_work_type;

/***********************************************************************
 * FC32 multiply const implementation
 **********************************************************************/
static int mult_fc32_work(
    int noutput_items,
    gr_vector_const_void_star &input_items,
    gr_vector_void_star &output_items,
    const void *val
){
    typedef std::complex<float> type;
    const type scalar = reinterpret_cast<const type *>(val)[0];
    type *out = reinterpret_cast<type *>(output_items[0]);
    const type *in = reinterpret_cast<const type *>(input_items[0]);
    volk_32fc_s32fc_multiply_32fc_a(out, in, scalar, noutput_items);
    return noutput_items;
}

/***********************************************************************
 * F32 multiply const implementation
 **********************************************************************/
static int mult_f32_work(
    int noutput_items,
    gr_vector_const_void_star &input_items,
    gr_vector_void_star &output_items,
    const void *val
){
    typedef float type;
    const type scalar = reinterpret_cast<const type *>(val)[0];
    type *out = reinterpret_cast<type *>(output_items[0]);
    const type *in = reinterpret_cast<const type *>(input_items[0]);
    volk_32f_s32f_multiply_32f_a(out, in, scalar, noutput_items);
    return noutput_items;
}

/***********************************************************************
 * Generic multiply const implementation
 **********************************************************************/
template <typename type>
class multiply_const_generic : public multiply_const_v{
public:
    multiply_const_generic(const std::vector<type> &vec, const volk_work_type volk_work = volk_work_type()):
        gr_sync_block(
            "multiply const generic",
            gr_make_io_signature (1, 1, sizeof(type)*vec.size()),
            gr_make_io_signature (1, 1, sizeof(type)*vec.size())
        ),
        _volk_work(volk_work)
    {
        _val.resize(vec.size());
        this->set_const(vec);
        if (volk_work){
            const int alignment_multiple = volk_get_alignment() / sizeof(type);
            set_output_multiple(std::max(1, alignment_multiple));
        }
    }

    int work(
        int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items
    ){
        if (_volk_work){
            _volk_work(noutput_items * _val.size(), input_items, output_items, &_val.front());
            return noutput_items;
        }

        const size_t n_nums = noutput_items * _val.size();
        type *out = reinterpret_cast<type *>(output_items[0]);
        const type *in = reinterpret_cast<const type *>(input_items[0]);

        //simple vec len 1 for the fast
        if (_val.size() == 1){
            const type val = _val[0];
            for (size_t i = 0; i < n_nums; i++){
                out[i] = in[i] * val;
            }
        }

        //general case for any vlen
        else{
            for (size_t i = 0; i < n_nums; i++){
                out[i] = in[i] * _val[i%_val.size()];
            }
        }
        return noutput_items;
    }

    void _set_const(const std::vector<std::complex<double> > &val){
        _original_val.assign(val.begin(), val.end());

        if (val.size() != _val.size()){
            throw std::invalid_argument("set_const called with the wrong length");
        }
        for (size_t i = 0; i < val.size(); i++){
            gr_complex_double_to_num(val[i], _val[i]);
        }
    }

    std::vector<std::complex<double> > get_const(void){
        return _original_val;
    }

private:
    std::vector<std::complex<double> > _original_val;
    std::vector<type> _val;
    const volk_work_type _volk_work;
};

/***********************************************************************
 * factory function
 **********************************************************************/
multiply_const_v::sptr multiply_const_v::make_fc32_fc32(const std::vector<std::complex<float> > &vec){
    return sptr(new multiply_const_generic<std::complex<float> >(vec, boost::bind(&mult_fc32_work, _1, _2, _3, _4)));
}

multiply_const_v::sptr multiply_const_v::make_f32_f32(const std::vector<float> &vec){
    return sptr(new multiply_const_generic<float>(vec, boost::bind(&mult_f32_work, _1, _2, _3, _4)));
}

#define make_factory_function(suffix, type) \
    multiply_const_v::sptr multiply_const_v::make_ ## suffix(const std::vector<type > &vec){ \
    return sptr(new multiply_const_generic<type >(vec)); \
}

make_factory_function(sc32_sc32, std::complex<int32_t>)
make_factory_function(sc16_sc16, std::complex<int16_t>)
make_factory_function(sc8_sc8, std::complex<int8_t>)
make_factory_function(s32_s32, int32_t)
make_factory_function(s16_s16, int16_t)
make_factory_function(s8_s8, int8_t)
