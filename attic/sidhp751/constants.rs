// This file is part of sidh-rs.
// Copyright (c) 2017 Erkan Tairi
// See LICENSE for licensing information.
//
// Author:
// - Erkan Tairi <erkan.tairi@gmail.com>
//

use crate::sidhp751::field::PrimeFieldElement;
use crate::sidhp751::fp::Fp751Element;

/// The x-coordinate of `P_A = [3^239](11, oddsqrt(11^3 + 11))` on `E_0(F_p)`.
pub const AFFINE_X_PA: PrimeFieldElement = PrimeFieldElement{ A: Fp751Element([0x27914862, 0xd56fe526, 0x96b5baea, 0x1fad60dc, 0xbf07ab91, 0x1e137d0, 0x52161964, 0x404d3e92, 0xcd09a337, 0x3c5385e4, 0x69e4af73, 0x44764267, 0x989dfe33, 0x9790c6db, 0xd2aa8b5e, 0xe06e1c04, 0xedea73b9, 0x38c08185, 0xa4396ca6, 0xaa41f678, 0x2229e9a0, 0x92b9259b, 0x26818be0, 0x2f93]) };

/// The y-coordinate of `P_A = [3^239](11, oddsqrt(11^3 + 11))` on `E_0(F_p)`.
pub const AFFINE_Y_PA: PrimeFieldElement = PrimeFieldElement{ A: Fp751Element([0xbe3d7739, 0x332bd16f, 0x2319e3db, 0x7e5e20ff, 0xaefbd81b, 0xea856234, 0x6d071283, 0xe016df7d, 0xf73cd34f, 0x8ae42796, 0xa4774575, 0x6364b408, 0x7ce99497, 0xa71c97f1, 0xaa0cbe71, 0xda03cdd9, 0x195bd56f, 0xe52b4fda, 0x1fce0a46, 0xdac41f81, 0x0ee84a61, 0x9333720f, 0xf006e578, 0x1399]) };

/// The x-coordinate of `P_B = [2^372](6, oddsqrt(6^3 + 6))` on `E_0(F_p)`.
pub const AFFINE_X_PB: PrimeFieldElement = PrimeFieldElement{ A: Fp751Element([0x7b96c4ab, 0xf1a8c9ed, 0x5178486e, 0x299429da, 0x0cd5c2f4, 0xef4926f2, 0x58b4716a, 0x683b2e28, 0x3cac3eeb, 0xdda2fbcc, 0x3a600460, 0xec055f9f, 0x58c3848b, 0xd5a5a17a, 0xf42eaed5, 0x4652d836, 0x78b3a3b3, 0x2f2e71ed, 0x180add1d, 0xa771c057, 0xd835f512, 0xc780a5d2, 0xa3b55ac1, 0x114e]) };

/// The y-coordinate of `P_B = [2^372](6, oddsqrt(6^3 + 6))` on `E_0(F_p)`.
pub const AFFINE_Y_PB: PrimeFieldElement = PrimeFieldElement{ A: Fp751Element([0x73e3736b, 0xd1e14712, 0x4da241fe, 0xf9301ba9, 0x7fef0a85, 0xe14ab3c1, 0x037e9e62, 0xb4ddd26a, 0xb2afeb69, 0x66142df, 0x649d6c9e, 0xe297cb70, 0x8b1a0912, 0x214dfc6e, 0xb01cf859, 0x9f5ba818, 0x07c12828, 0x87d15b49, 0x3a880dbf, 0xa4da70c5, 0x72c8f253, 0xac5df62a, 0xa42ec617, 0x2e26]) };

/// The value of `(a+2)/4` for the starting curve `E_0` with `a=0`: this is `1/2`.
pub const E0_A_PLUS2_OVER4: PrimeFieldElement = PrimeFieldElement{ A: Fp751Element([0x124d6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xb8e00000, 0xc0aa7287, 0x9c8a2434, 0xa9a378a3, 0xa206996c, 0x41a41b52, 0x6876280d, 0x175ce04f, 0xe903b49f, 0x0666d227, 0xf851186, 0x7cff6e7f, 0x4ea0]) };
