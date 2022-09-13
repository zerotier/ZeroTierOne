// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

#[cfg(any(target_os = "macos", target_os = "ios"))]
#[allow(unused)]
mod impl_macos;

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
#[allow(unused)]
mod impl_openssl;

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub use crate::aes_gmac_siv::impl_macos::AesGmacSiv;

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
pub use crate::aes_gmac_siv::impl_openssl::AesGmacSiv;

#[cfg(test)]
mod tests {
    use crate::aes_gmac_siv::AesGmacSiv;
    use sha2::Digest;
    use std::time::SystemTime;

    const TV0_KEYS: [&'static [u8]; 2] = ["00000000000000000000000000000000".as_bytes(), "11111111111111111111111111111111".as_bytes()];

    /// Test vectors consist of a series of input sizes, a SHA384 hash of a resulting ciphertext, and an expected tag.
    /// Input is a standard byte array consisting of bytes 0, 1, 2, 3, ..., 255 and then cycling back to 0 over and over
    /// and is provided both as ciphertext and associated data (AAD).
    #[allow(unused)]
    const TEST_VECTORS: [(usize, &'static str, &'static str); 85] = [
        (0, "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b", "43847e644239134deccf5538162c861e"),
        (777, "aabf892f18a620b9c3bae91bb03a74c84193e4a7b64916c6bc88b885b9ebed4134495e5f22f12e3046fbb3f26fa111a7", "b8c318b5dcc1d672114a6f7be54ef289"),
        (1554, "648f551df29217f0e634b72ba6973c0eb95c7d4be8b135e550d8bcdf65b75980881bc0e03cf22589e04bedc7da1804cd", "535b8ddd51ec82a1e850906fe321b21a"),
        (2331, "bfbfdffea40062e23bbdf0835e1d38d1623bebca7407908bbc6d5b3f2bfd062a2d237f091affda7348094fafda0bd1a7", "4f521876fbb2c563051196b33c20c822"),
        (3108, "cc6035cab70f3a3298a5c4956ff07f179acf3771bb915c590a8a19fe5133d6d8a81c118148394dfb364af5c2fbdaadeb", "d3adfa578c8bcd738c55ffc527358cef"),
        (3885, "15ec2760a21c25f9870a84ee757f3da2c261a950c2f692d75ff9e99b2d50c826c21e27e49c4cd3450fedc7e60371589f", "a4c22d6c3d773634c2dc057e1f7c6738"),
        (4662, "c2afad6f034704300c34f143dcdcb86c9b954cec1ebf22e7071f288c58a2ae430d3e3748d214d1021472793d3f337dc6", "c0601cb6cd4883102f70570c2cdc0ab6"),
        (5439, "8fee067f5a7a475a630f9db8b2eb80c1edc40eb4246a0f1c078e535df7d06451c6a9bde1a23ba70285690dd7100a8626", "7352239f2302b08844309d28b13fa867"),
        (6216, "60095b4172438aee61e65f5379f4ef276c3632d4ac74eea7723a2201823432614aba7b4670d9bf7a5b9126ca38f3b88a", "c0f0b0aa651965f8514b473c5406285e"),
        (6993, "10e754dd08b4d2a6c109fb01fce2b57d54743947e14a7e67d7efd0608baf91f7fc42a53328fe8c18d234abad8ebcdff0", "58444988a62a99060728a7637c8499eb"),
        (7770, "1abc4a5dcd2696336bd0e8af20fe7fc261aa424b52cfb5ad80ee7c7c793ac44f11db3506cdbbbaed0f80000925d08d52", "e8065c563bc6018cdcbf9aaafef767e6"),
        (8547, "26aaf74ae8bfc6aaf45ceee0476ea0a484304f5c36050d3e2265cb194a2f7c308213314232270608b6d3f1c11b834e33", "ec50e4b3f6e4b3de24b3476623d08157"),
        (9324, "863206305d466aa9c0d0ec674572069f61fe5009767f99ec8832912725c28c49d6a106ad3f55372c922e4e169fc382ce", "0cfac64f49e0f128d0a18d293878f222"),
        (10101, "bd0c0950b947a6c34f1fa6e877433b42c039a8ea7b37634c40fb47efae4958ba74ef0991cfedf3c82a0b87ef59635071", "e0220a02b74259eeebbebede847d50f9"),
        (10878, "d7b9901af1dacf6a8c369b993ba1c607f9b7f073d02311c72d8449d3494d477ffc8344a1d8b488020ccfc7c80fbd27e1", "ebe3933146734a6ade2b434f2bcd78ae"),
        (11655, "0ba265e3ef0bebf01a4f3490da462c7730aad6aa6c70bb9ce64a36d26d24fe213660e60e4d3301329170471f11ff8ca2", "ec3dd4bf4cb7d527a86dd559c773a87b"),
        (12432, "c3b6755a1be922ec71c1e187ead36c4e6fc307c72969c64ca1e9b7339d61e1a93a74a315fd73bed8fa5797b78b19dbe5", "5b58dcf392749bcef91056ba9475d0ef"),
        (13209, "2fb1a67151183daa2f0d7f0064534497357f173161349dd008499a8c1a123cc942662ecc426e2ad7743fe0ab9f5d7be1", "c011260d328d310e2ab606aa1ef8afd4"),
        (13986, "6afae2a07ce9bfe30fbbfb7dcf32d755bcf357334dc5c309e58cab38ebe559f25b313a0b3ca32ff1dc41f7b99718f653", "011bf43cfbbb7ae5986f8e0fc87771a9"),
        (14763, "cc6215c115eb6411f4712c2289f5bf0ccb5151635f9f9ceac7c1b62d8d2f4d26498079d0289f83aeb26e97b5b924ffc4", "a015034a8d5bc83cc76c6983a5ba19ab"),
        (15540, "3cebce794e947341c4ceec444ca43c6ac57c6f58de462bfec7566cbd59a1b6f2eae774120e29521e76120a604d1a12d9", "d373cd2bd9000655141ac632880eca40"),
        (16317, "899147b98d78bb5d137dc7c4f03be7eca82bcca19cc3a701261332923707aed2e6719d35d2f2bf067cd1d193a53529cf", "ed223b64529299c787f49d631ce181c1"),
        (17094, "aecd1830958b994b2c331b90e7d8ff79f27c83a71f5797a65ade3a30b4fa5928e79140bcd03f375591d53df96fea1a4d", "948a7c253d54bb6b65d78530c0eb7aab"),
        (17871, "e677ffd4ecaba5899659fefe5fe8e643004392be3be6dc5a801409870ac1e3398f47cc1d83f7a4c41925b6337e01f7fd", "156a600c336f3ac034ca90034aa22635"),
        (18648, "4ee50f4a98d0bbd160add6acf76765ccdac0c1cd0bb2adbbcb22dd012a1121620b739a120df7dc4091e684ddf28eb726", "75873467b416a7b025f9f1b015bf653a"),
        (19425, "aa025f32c0575af7209828fc7fc4591b41fa7cfb485e26c5401e63ca1fa05776f8b8af1769a15e81f2c663bca9b02ab3", "5679efa7a4404e1e5c9b372782a41bf2"),
        (20202, "6e77ab62d2affeb27f4ef326191b3df3863c338a629f64a785505f4a5968ff59bc011c7a27951cb00e2e7d9b9bd32fec", "36a9c4515d34f9bb962d8876ab3b5c86"),
        (20979, "1625b4f0e65fc66f11ba3ee6b3e20c732535654c447df6b517ced113107a1057a64477faa2af4a5ede4034bf3cff98ea", "9058044e0f71c28d4f8d3281a3aec024"),
        (21756, "94efe6aa55bd77bfa58c185dec313a41003f9bef02568e72c337be4de1b46c6e5bb9a9329b4f108686489b8bc9d5f4f0", "8d6d2c90590268a26f5e7d76351f48c1"),
        (22533, "7327a05fdb0ac92433dfc2c85c5e96e6ddcbdb01e079f8dafbee79c14cb4d5fd46047acd6bb0e09a98f6dd03dced2a0a", "4e0f0a394f85bca35c68ef667aa9c244"),
        (23310, "93da9e356efbc8b5ae366256f4c6fc11c11fc347aaa879d591b7c1262d90adf98925f571914696054f1d09c74783561e", "8c83c157be439280afc790ee3fd667eb"),
        (24087, "99b91be5ffca51b1cbc7410798b1540b5b1a3356f801ed4dc54812919c08ca5a9adc218bc51e594d97b46445a1515506", "9436ff05729a77f673e815e464aeaa75"),
        (24864, "074253ad5d5a5d2b072e7aeaffa04a06119ec812a88ca43481fe5e2dce02cf6736952095cd342ec70b833c12fc1777f4", "69d8951b96866a08efbb65f2bc31cfbc"),
        (25641, "c0a301f90597c05cf19e60c35378676764086b7156e455f4800347f8a6e733d644e4cc709fb9d95a9211f3e1e10c762a", "3561c9802143c306ecc5e07e3b976d9e"),
        (26418, "3c839e59d945b841acb604e1b9ae3df36a291444ce0bcae336ee875beaf208bf10af7342b375429ecb92ec54d11a5907", "3032ffdb8daee11b2e739132c6175615"),
        (27195, "3dc59b16603950dfc26a90bf036712eb088412e8de4d1b27c3fa6be6502ac12d89d194764fb53c3dc7d90fa696ba5a16", "49436717edff7cd67c9a1be16d524f07"),
        (27972, "4fbc0d40ff13376b8ed5382890cdea337b4a0c9c31b477c4008d2ef8299bd5ab771ba70b1b4b743f8f7caa1f0164d1a1", "64a9856a3bb81dc81ff1bc1025192dc9"),
        (28749, "6ab191aa6327f229cc94e8c7b1b7ee30bc723e6aeaf3050eb7d14cb491c3513254e9b19894c2b4f071d298401fd31945", "101f2ffea60f246a3b57c4a530d67cf1"),
        (29526, "d06dece58e6c7345986aae4b7f15b3317653f5387d6262f389b5cbbe804568124a876eabb89204e96b3c0f7b552df3c4", "5c0e873adba65a9f4cb24cce4f194b18"),
        (30303, "7a33c1268eafdc1f89ad460fa4ded8d3df9a3cabe4339706877878c64a2c8080cf3fa5ea7f2f24744e3341476b1eb5a5", "b7dc708fc46ce5cde24a31ad549fec83"),
        (31080, "37bf1f9fca6d705b989b2d63259ca924dc860fc6027e07d9aad79b94841227739774f5d324590df45d8f41249ef742ea", "8ead50308c281e699b79b69dad7ecb91"),
        (31857, "91b120c73be86f9d53326fa707cfa1411e5ac76ab998a2d7ebd73a75e3b1a04c9f0855d102184b8a3fd5d99818b0b134", "6056d09595bd16bfa317c6f87ce64bb7"),
        (32634, "42cc255c06184ead57b27efd0cefb0f2c788c8962a6fd15db3f25533a7f49700bca85af916f9e985f1941a6e66943b38", "3b15e332d2f53bb97e1a9d03e6113b97"),
        (33411, "737f8bb8f3fd03a9d13e50abba3a42f4491c36eda3eb215085abda733227ec490cb863ffbd68f915c8fb2926a899fbc3", "b2c647d25c46aab4d4a5ede4a3b4576d"),
        (34188, "e9caa36505e19628175d1ce8b933267380099753a41e503fa2f894cea17b7692f0b27079ed33cdd1293db9a35722d561", "a2882adfd00f22823250215b12b3a1fd"),
        (34965, "81ddc348ebbdfb963daa5d0c1b51bbb73cacd883d4fc4316db6bd3388779beff7be0655bbac73951f89dc53832199c11", "f33106eb8104f3780350c6d4f82333ad"),
        (35742, "308ce31daf40dab707e2cb4c4a5307bc403e24c971ae1e30e998449f804a167fe5f2cf617d585851b6fe9f2b4209f09c", "44070ac90cbf350ab92289cc063e978c"),
        (36519, "71f51b4bddbe8a52f18be75f9bdb3fca0773901b794de845450fb308c34775ede1a6da9a82b61e9682a29a3ef71274e2", "0e387704298c444bf3afba0edc0c1c1c"),
        (37296, "478ac94eee8c5f96210003fcb478392b91f2ef6fc3a729774e5fe82a2d8d0abc54ae1d25b3eaefb061e2bd43b70ca4ea", "fb65ebeda52cd5848d303c0677cecb7f"),
        (38073, "bc3a9390618da7d644be932627353e2c92024df939d2d8497fba61fae3dd822cdd3e130c1707f4a9d5d4a0cbb4b3e0b3", "d790d529a837ec79f7cc3f66ed9a399f"),
        (38850, "ef0e63a53a10e56477c47e13320b8a7d330aee3a4363c850edc56c0707a2686478e5a5193f54ceb33467ab7e8a22aa21", "6f2c18742f106f16fc290767342fb62b"),
        (39627, "c16f63533c099d872d9a01c326db7756e7eb488c756b9a6ebf575993d8ea2eb45c572b2e162f061e145710e0e21e8e18", "a57afde7938b223ae5e109a03db4ee4c"),
        (40404, "ade484ae8c13465a73589ef14789bb6891c933453e198df84edd34b4ac5c83aa90f2cf61fa072fa4d8f5b5c4cd68fa9e", "a01d13009db86ac442f7afd39d83309f"),
        (41181, "6c5c7eed0e043a0bd60bcac9b5b546e150028d70c1efefc9ff69037ef4dc1a36878b171b9f2a639df822d11054a0e405", "6321c8622ca5866c875d340206d06a28"),
        (41958, "dd311c54222fb0d92858719cf5b1c51bb5e3ca2539ffd68f1dd6c7e38969495be935804855ccdcc4b4cf221fcdbda886", "cf401eb819b5dc5cd8c909aae9b3b34b"),
        (42735, "31cda9d663199b32eff042dd16c0b909ba999641e77ba751c91752bfc4d595e17ec6467119e74a600b72da72ba287d0a", "12fd6298ab5d744eb6ade3106565afad"),
        (43512, "11b014057d51a8384d549d5d083c4406b575df6a9295853dd8f2f84f078cc241bb90495a119126b10b9510efcb68c0d3", "a48a49eea5dc90359ef21f32132f8604"),
        (44289, "b44f5dbeecd76ee7efe3fb4dfe10ba8135d7a5e4d104149f4a91c5c6ee9446d9be19fb4c9ba668b074466d3892e22228", "07e1cbb7a19174d9b1e4d5a2c741cc14"),
        (45066, "d87bbba3a3c739cab622386c89aeb685a70009fab1a606bd34622adfa3a75a05b58d56ee6b9874d414db38a6a32927b3", "a27cd252712cd2a1a2d95dea39f888d4"),
        (45843, "abb90e60ea13c6cb3b401b8e271637416b87fbede165dde7be1d34abe4427dae4b39b499352cacac909bc43fb94028c8", "df3ae762b9257936feda435a61a9c3a1"),
        (46620, "56d1132ee6e0f85543950d2d9667244b66b0ce6414eacd1859b128ed0b9026b31a25bfdcce3d1a0ce7c39d99f609c89c", "cfe7c3c3f1cb615e2d210cc8136443e6"),
        (47397, "ecb023ec4c23cf95d1848a38b359f1f590f172dee9d8fb1be6bc9c4fb2ce96f612d60d7b111de539ab8313a87b821176", "501d24752bf55cb12239863981898a07"),
        (48174, "34236ab60f05bb510aa0880fec358fb2002903efa14c912cab8a399e09418f97223ca2f7b8d6798c11d39e79032eaaa8", "4ecaba4eae886aa429927188abab9623"),
        (48951, "55e8b40fad90a3d8c85a0f4d5bcf5975b8a6e2fb78377109f5b607a5e367187fbbc9a1e978aab3228fbf43ad23d0ad13", "84c43bc30eb4a67230b6c634fe3c7782"),
        (49728, "14b1f896d0d01ecff4e456c3c392b1ca2bad9f1ef07713f84cdd89e663aa27ca77d80213ed57a89431eb992b11d98749", "7f58c2f9a249f70fe1c6f9b4f65e5a1d"),
        (50505, "1335b1fb56196e0b371fa53ab7445845fdefcea3eb2833478deb3526e2ec888945e95ee8239b52caae5b9920ba4f43bb", "5fd729126b236ce3e0686fc706dce20f"),
        (51282, "0d1983a6cab870c5e78f89a11dd30e7d2c71a3882f8bba3e71dc1b96a2d9fc6cc6d91d683b74456b886de34df792cfda", "7731ae6e6c54dfde12f6116357e812ea"),
        (52059, "9d619fb4aa8441baaefed7b778693c291f2c1441b206ec135930fac3529d26587ac36f4472949e0b198b51c0c5a9d0f8", "39db2c996aea28996e03d576c118630f"),
        (52836, "31dca4fa285878ba3efc3b66a248a078b69a11c3c73f81077377c4ffcb7002627aad5faa955e3141c1d8508aad68c8f6", "32ac1e5a09e7e629ff95f30aa9b69c00"),
        (53613, "931a9969cf2bb02302c32b1eecd4933805e2da403d85aaf98c82c68129fb95f089eb85c65a6fcbc7d81bedb39de0cabb", "1a6f54b87c12868da530eac94d99eb31"),
        (54390, "2f0742565801a37810ecb3f50a6f782e73a369a790d1a6a85135e7ffa12fc063db8909ab9eca7cf7308832887a6149d1", "1b18ed6a8f901b7947626216839f0643"),
        (55167, "901defbd308b54deef89acd0d94e4387b370f9d2e6f870d72da2e447ed3ebe69c5f9f144488bd6207a732102160bff47", "1e0e6a05fcc0794121f617e28cfac1a0"),
        (55944, "df984a5f7475250155dc4733a746e98446dc93a56a3f3bff691ddfef7deefb32b1da1b0e7e15cce443831ebfb3e30ada", "876121af882d0ebeae38f111f3d4b6e8"),
        (56721, "acb693ed837b33561408cb1eed636e0082ac404f3fd72d277fa146ae5cd81a1fde3645f4cdc7babd8ba044b78075cb67", "5b90ed6c7943fc6da623c536e2ff1352"),
        (57498, "dffb54bf5938e812076cfbf15cd524d72a189566c7980363a49dd89fb49e230d9742ef0b0e1ac543dca14366d735d152", "22aee072457306e32747fbbbc3ae127c"),
        (58275, "92dbc245a980fc78974f7a27e62c22b12a00be9d3ef8d3718ff85f6d5fbcbf1d9d1e0f0a3daeb8c2628d090550a0ff6b", "5fa348117faba4ac8c9d9317ff44cd2d"),
        (59052, "57721475cb719691850696d9a8ad4c28ca8ef9a7d45874ca21df4df250cb87ea60c464f4e3252e2d6161ed36c4b56d75", "24d92ae7cac56d9c0276b06f7428d5df"),
        (59829, "d0936026440b5276747cb9fb7dc96de5d4e7846c233ca5f6f9354b2b39f760333483cbe99ffa905facb347242f58a7ef", "05c57068e183f9d835e7f461202f923c"),
        (60606, "7b3bb3527b73a8692f076f6a503b2e09b427119543c7812db73c7c7fb2d43af9ecbd2a8a1452ac8ada96ad0bad7bb185", "f958635a193fec0bfb958e97961381df"),
        (61383, "ff0d00255a36747eced86acfccd0cf9ef09faa9f44c8cf382efec462e7ead66e562a971060c3f32798ba142d9e1640a2", "838159b222e56aadde8229ed56a14095"),
        (62160, "15806e088ed1428cd73ede3fecf5b60e2a616f1925004dadd2cab8e847059f795659659e82a4554f270baf88bf60af63", "fed2aa0c9c0a73d499cc970aef21c52f"),
        (62937, "cfad71b23b6da51256bd1ddbd1ac77977fe10b2ad0a830a23a794cef914bf71a9519d78a5f83fc411e8d8db996a45d4e", "e1ea412fd3e1bd91c24b6b6445e8ff43"),
        (63714, "7d03a3698a79b1af1663e3e485c2efdc306ecd87b2644f2e01d83a35999d6cdf12241b6114d60d107c10c0d0c9cc0d23", "e6a3c3f3fd2d9cfcdc06cca2f59e9a83"),
        (64491, "e12b168cce0e82ed1db88df549f39b3ff40b5884a09fceae69c4c3db13c1c37ea79531c47b2700d1c27774a1ab7e8b35", "4cbb14d789f5cd8eca49ce9e1d442ea1"),
        (65268, "056c9d1172cfa76ce7f19c605e5969c284b82dca155dc9c1ed58062ab4d5a7704e27fe69f3aa745b73f45f1cd0ee57df", "8195187f092d52c2a8695b680568b934"),
    ];

    fn to_hex(b: &[u8]) -> String {
        let mut s = String::new();
        for c in b.iter() {
            s = format!("{}{:0>2x}", s, *c);
        }
        s
    }

    /// Run a bunch of test vectors.
    #[test]
    fn test_vectors() {
        let mut test_pt = [0_u8; 65536];
        let mut test_ct = [0_u8; 65536];
        let mut test_aad = [0_u8; 65536];
        for i in 0..65536 {
            test_pt[i] = i as u8;
            test_aad[i] = i as u8;
        }
        let mut c = AesGmacSiv::new(TV0_KEYS[0], TV0_KEYS[1]);
        for (test_length, expected_ct_sha384, expected_tag) in TEST_VECTORS.iter() {
            test_ct.fill(0);
            c.reset();
            c.encrypt_init(&(*test_length as u64).to_le_bytes());
            c.encrypt_set_aad(&test_aad[0..*test_length]);
            c.encrypt_first_pass(&test_pt[0..*test_length]);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass(&test_pt[0..*test_length], &mut test_ct[0..*test_length]);
            let tag = c.encrypt_second_pass_finish();
            let ct_hash = sha2::Sha384::digest(&test_ct[0..*test_length]).to_vec();
            //println!("{} {} {}", *test_length, to_hex(ct_hash.as_slice()), to_hex(tag));
            if !to_hex(ct_hash.as_slice()).eq(*expected_ct_sha384) {
                panic!("test vector failed (ciphertest)");
            }
            if !to_hex(tag).eq(*expected_tag) {
                panic!("test vector failed (tag)");
            }
        }
    }

    /// Test repeated encrypt/decrypt and run a benchmark. Run with --nocapture to see it.
    #[test]
    fn encrypt_decrypt() {
        let aes_key_0: [u8; 32] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32];
        let aes_key_1: [u8; 32] = [2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32];
        let iv: [u8; 8] = [0, 1, 2, 3, 4, 5, 6, 7];

        let mut buf = [0_u8; 12345];
        for i in 1..12345 {
            buf[i] = i as u8;
        }

        let mut c = AesGmacSiv::new(&aes_key_0, &aes_key_1);

        for _ in 0..256 {
            c.reset();
            c.encrypt_init(&iv);
            c.encrypt_first_pass(&buf);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass_in_place(&mut buf);
            let tag = c.encrypt_second_pass_finish().clone();
            let sha = sha2::Sha384::digest(&buf).to_vec();
            let sha = to_hex(sha.as_slice());
            if sha != "4dc97c10abb6112a3907e5eb588ea5123719442b715da994d9756b003677719824326973960268823d924f66491a16e6" {
                panic!("encrypt result hash check failed! {}", sha);
            }
            //println!("Encrypt OK, tag: {}, hash: {}", to_hex(&tag), sha);

            c.reset();
            c.decrypt_init(&tag);
            c.decrypt_in_place(&mut buf);
            let _ = c.decrypt_finish().expect("decrypt_finish() failed!");
            for i in 1..12345 {
                if buf[i] != (i & 0xff) as u8 {
                    panic!("decrypt data check failed!");
                }
            }
            //println!("Decrypt OK");
        }
        println!("Encrypt/decrypt test OK");

        let benchmark_iterations: usize = 80000;
        let start = SystemTime::now();
        for _ in 0..benchmark_iterations {
            c.reset();
            c.encrypt_init(&iv);
            c.encrypt_first_pass(&buf);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass_in_place(&mut buf);
            let _ = c.encrypt_second_pass_finish();
        }
        let duration = SystemTime::now().duration_since(start).unwrap();
        println!("AES-GMAC-SIV (legacy) encrypt benchmark: {} MiB/sec", (((benchmark_iterations * buf.len()) as f64) / 1048576.0) / duration.as_secs_f64());
        let start = SystemTime::now();
        for _ in 0..benchmark_iterations {
            c.reset();
            c.decrypt_init(&buf[0..16]); // we don't care if decryption is successful to benchmark, so anything will do
            c.decrypt_in_place(&mut buf);
            c.decrypt_finish();
        }
        let duration = SystemTime::now().duration_since(start).unwrap();
        println!("AES-GMAC-SIV (legacy) decrypt benchmark: {} MiB/sec", (((benchmark_iterations * buf.len()) as f64) / 1048576.0) / duration.as_secs_f64());
    }
}
