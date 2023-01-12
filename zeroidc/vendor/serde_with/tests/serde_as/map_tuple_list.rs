use super::*;
use std::net::IpAddr;

#[test]
fn test_map_as_tuple_list() {
    let ip = "1.2.3.4".parse().unwrap();
    let ip2 = "255.255.255.255".parse().unwrap();

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SB(#[serde_as(as = "Vec<(DisplayFromStr, DisplayFromStr)>")] BTreeMap<u32, IpAddr>);

    let map: BTreeMap<_, _> = vec![(1, ip), (10, ip), (200, ip2)].into_iter().collect();
    is_equal(
        SB(map.clone()),
        expect![[r#"
            [
              [
                "1",
                "1.2.3.4"
              ],
              [
                "10",
                "1.2.3.4"
              ],
              [
                "200",
                "255.255.255.255"
              ]
            ]"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SB2(#[serde_as(as = "Vec<(Same, DisplayFromStr)>")] BTreeMap<u32, IpAddr>);

    is_equal(
        SB2(map),
        expect![[r#"
            [
              [
                1,
                "1.2.3.4"
              ],
              [
                10,
                "1.2.3.4"
              ],
              [
                200,
                "255.255.255.255"
              ]
            ]"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SH(#[serde_as(as = "Vec<(DisplayFromStr, DisplayFromStr)>")] HashMap<u32, IpAddr>);

    // HashMap serialization tests with more than 1 entry are unreliable
    let map1: HashMap<_, _> = vec![(200, ip2)].into_iter().collect();
    let map: HashMap<_, _> = vec![(1, ip), (10, ip), (200, ip2)].into_iter().collect();
    is_equal(
        SH(map1.clone()),
        expect![[r#"
            [
              [
                "200",
                "255.255.255.255"
              ]
            ]"#]],
    );
    check_deserialization(
        SH(map.clone()),
        r#"[["1","1.2.3.4"],["10","1.2.3.4"],["200","255.255.255.255"]]"#,
    );
    check_error_deserialization::<SH>(
        r#"{"200":"255.255.255.255"}"#,
        expect![[r#"invalid type: map, expected a sequence at line 1 column 0"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SH2(#[serde_as(as = "Vec<(Same, DisplayFromStr)>")] HashMap<u32, IpAddr>);

    is_equal(
        SH2(map1),
        expect![[r#"
            [
              [
                200,
                "255.255.255.255"
              ]
            ]"#]],
    );
    check_deserialization(
        SH2(map),
        r#"[[1,"1.2.3.4"],[10,"1.2.3.4"],[200,"255.255.255.255"]]"#,
    );
    check_error_deserialization::<SH2>(
        r#"1"#,
        expect![[r#"invalid type: integer `1`, expected a sequence at line 1 column 1"#]],
    );
}

#[test]
fn test_tuple_list_as_map() {
    let ip = "1.2.3.4".parse().unwrap();
    let ip2 = "255.255.255.255".parse().unwrap();

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SH(#[serde_as(as = "HashMap<DisplayFromStr, DisplayFromStr>")] Vec<(u32, IpAddr)>);

    is_equal(
        SH(vec![(1, ip), (10, ip), (200, ip2)]),
        expect![[r#"
            {
              "1": "1.2.3.4",
              "10": "1.2.3.4",
              "200": "255.255.255.255"
            }"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SB(#[serde_as(as = "BTreeMap<DisplayFromStr, DisplayFromStr>")] Vec<(u32, IpAddr)>);

    is_equal(
        SB(vec![(1, ip), (10, ip), (200, ip2)]),
        expect![[r#"
            {
              "1": "1.2.3.4",
              "10": "1.2.3.4",
              "200": "255.255.255.255"
            }"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SD(#[serde_as(as = "BTreeMap<DisplayFromStr, DisplayFromStr>")] VecDeque<(u32, IpAddr)>);

    is_equal(
        SD(vec![(1, ip), (10, ip), (200, ip2)].into()),
        expect![[r#"
            {
              "1": "1.2.3.4",
              "10": "1.2.3.4",
              "200": "255.255.255.255"
            }"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct Sll(
        #[serde_as(as = "HashMap<DisplayFromStr, DisplayFromStr>")] LinkedList<(u32, IpAddr)>,
    );

    is_equal(
        Sll(vec![(1, ip), (10, ip), (200, ip2)].into_iter().collect()),
        expect![[r#"
            {
              "1": "1.2.3.4",
              "10": "1.2.3.4",
              "200": "255.255.255.255"
            }"#]],
    );

    #[serde_as]
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    struct SO(#[serde_as(as = "HashMap<DisplayFromStr, DisplayFromStr>")] Option<(u32, IpAddr)>);

    is_equal(
        SO(Some((1, ip))),
        expect![[r#"
            {
              "1": "1.2.3.4"
            }"#]],
    );
    is_equal(SO(None), expect![[r#"{}"#]]);
}

#[test]
fn test_tuple_array_as_map() {
    #[serde_as]
    #[derive(Debug, PartialEq, Serialize, Deserialize)]
    struct S1(#[serde_as(as = "BTreeMap<_, _>")] [(u8, u8); 1]);
    is_equal(
        S1([(1, 2)]),
        expect![[r#"
            {
              "1": 2
            }"#]],
    );

    #[serde_as]
    #[derive(Debug, PartialEq, Serialize, Deserialize)]
    struct S2(#[serde_as(as = "HashMap<_, _>")] [(u8, u8); 33]);
    is_equal(
        S2([
            (0, 0),
            (1, 1),
            (2, 2),
            (3, 3),
            (4, 4),
            (5, 5),
            (6, 6),
            (7, 7),
            (8, 8),
            (9, 9),
            (10, 10),
            (11, 11),
            (12, 12),
            (13, 13),
            (14, 14),
            (15, 15),
            (16, 16),
            (17, 17),
            (18, 18),
            (19, 19),
            (20, 20),
            (21, 21),
            (22, 22),
            (23, 23),
            (24, 24),
            (25, 25),
            (26, 26),
            (27, 27),
            (28, 28),
            (29, 29),
            (30, 30),
            (31, 31),
            (32, 32),
        ]),
        expect![[r#"
            {
              "0": 0,
              "1": 1,
              "2": 2,
              "3": 3,
              "4": 4,
              "5": 5,
              "6": 6,
              "7": 7,
              "8": 8,
              "9": 9,
              "10": 10,
              "11": 11,
              "12": 12,
              "13": 13,
              "14": 14,
              "15": 15,
              "16": 16,
              "17": 17,
              "18": 18,
              "19": 19,
              "20": 20,
              "21": 21,
              "22": 22,
              "23": 23,
              "24": 24,
              "25": 25,
              "26": 26,
              "27": 27,
              "28": 28,
              "29": 29,
              "30": 30,
              "31": 31,
              "32": 32
            }"#]],
    );
}
