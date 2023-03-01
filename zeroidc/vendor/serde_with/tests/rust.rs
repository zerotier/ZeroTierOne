#![allow(
    // clippy is broken and shows wrong warnings
    // clippy on stable does not know yet about the lint name
    unknown_lints,
    // https://github.com/rust-lang/rust-clippy/issues/8867
    clippy::derive_partial_eq_without_eq,
)]

extern crate alloc;

mod utils;

use crate::utils::{check_deserialization, check_error_deserialization, is_equal};
use alloc::collections::{BTreeMap, BTreeSet, LinkedList, VecDeque};
use core::{cmp, iter::FromIterator as _};
use expect_test::expect;
use fnv::{FnvHashMap as HashMap, FnvHashSet as HashSet};
use pretty_assertions::assert_eq;
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use serde_with::CommaSeparator;

#[test]
fn string_collection() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "serde_with::rust::StringWithSeparator::<CommaSeparator>")] Vec<String>,
    );

    is_equal(S(vec![]), expect![[r#""""#]]);
    is_equal(
        S(vec![
            "A".to_string(),
            "B".to_string(),
            "c".to_string(),
            "D".to_string(),
        ]),
        expect![[r#""A,B,c,D""#]],
    );
    is_equal(
        S(vec!["".to_string(), "".to_string(), "".to_string()]),
        expect![[r#"",,""#]],
    );
    is_equal(
        S(vec!["AVeryLongString".to_string()]),
        expect![[r#""AVeryLongString""#]],
    );
}

#[test]
fn prohibit_duplicate_value_hashset() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")] HashSet<usize>);

    is_equal(
        S(HashSet::from_iter(vec![1, 2, 3, 4])),
        expect![[r#"
            [
              4,
              1,
              3,
              2
            ]"#]],
    );
    check_error_deserialization::<S>(
        r#"[1, 2, 3, 4, 1]"#,
        expect![[r#"invalid entry: found duplicate value at line 1 column 15"#]],
    );
}

#[test]
fn prohibit_duplicate_value_btreeset() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")] BTreeSet<usize>);

    is_equal(
        S(BTreeSet::from_iter(vec![1, 2, 3, 4])),
        expect![[r#"
            [
              1,
              2,
              3,
              4
            ]"#]],
    );
    check_error_deserialization::<S>(
        r#"[1, 2, 3, 4, 1]"#,
        expect![[r#"invalid entry: found duplicate value at line 1 column 15"#]],
    );
}

#[test]
fn prohibit_duplicate_key_hashmap() {
    #[derive(Debug, Eq, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")] HashMap<usize, usize>,
    );

    // Different value and key always works
    is_equal(
        S(HashMap::from_iter(vec![(1, 1), (2, 2), (3, 3)])),
        expect![[r#"
            {
              "1": 1,
              "3": 3,
              "2": 2
            }"#]],
    );

    // Same value for different keys is ok
    is_equal(
        S(HashMap::from_iter(vec![(1, 1), (2, 1), (3, 1)])),
        expect![[r#"
            {
              "1": 1,
              "3": 1,
              "2": 1
            }"#]],
    );

    // Duplicate keys are an error
    check_error_deserialization::<S>(
        r#"{"1": 1, "2": 2, "1": 3}"#,
        expect![[r#"invalid entry: found duplicate key at line 1 column 24"#]],
    );
}

#[test]
fn prohibit_duplicate_key_btreemap() {
    #[derive(Debug, Eq, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")] BTreeMap<usize, usize>,
    );

    // Different value and key always works
    is_equal(
        S(BTreeMap::from_iter(vec![(1, 1), (2, 2), (3, 3)])),
        expect![[r#"
            {
              "1": 1,
              "2": 2,
              "3": 3
            }"#]],
    );

    // Same value for different keys is ok
    is_equal(
        S(BTreeMap::from_iter(vec![(1, 1), (2, 1), (3, 1)])),
        expect![[r#"
            {
              "1": 1,
              "2": 1,
              "3": 1
            }"#]],
    );

    // Duplicate keys are an error
    check_error_deserialization::<S>(
        r#"{"1": 1, "2": 2, "1": 3}"#,
        expect![[r#"invalid entry: found duplicate key at line 1 column 24"#]],
    );
}

#[test]
fn duplicate_key_first_wins_hashmap() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::maps_first_key_wins")] HashMap<usize, usize>);

    // Different value and key always works
    is_equal(
        S(HashMap::from_iter(vec![(1, 1), (2, 2), (3, 3)])),
        expect![[r#"
            {
              "1": 1,
              "3": 3,
              "2": 2
            }"#]],
    );

    // Same value for different keys is ok
    is_equal(
        S(HashMap::from_iter(vec![(1, 1), (2, 1), (3, 1)])),
        expect![[r#"
            {
              "1": 1,
              "3": 1,
              "2": 1
            }"#]],
    );

    // Duplicate keys, the first one is used
    check_deserialization(
        S(HashMap::from_iter(vec![(1, 1), (2, 2)])),
        r#"{"1": 1, "2": 2, "1": 3}"#,
    );
}

#[test]
fn duplicate_key_first_wins_btreemap() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::maps_first_key_wins")] BTreeMap<usize, usize>);

    // Different value and key always works
    is_equal(
        S(BTreeMap::from_iter(vec![(1, 1), (2, 2), (3, 3)])),
        expect![[r#"
            {
              "1": 1,
              "2": 2,
              "3": 3
            }"#]],
    );

    // Same value for different keys is ok
    is_equal(
        S(BTreeMap::from_iter(vec![(1, 1), (2, 1), (3, 1)])),
        expect![[r#"
            {
              "1": 1,
              "2": 1,
              "3": 1
            }"#]],
    );

    // Duplicate keys, the first one is used
    check_deserialization(
        S(BTreeMap::from_iter(vec![(1, 1), (2, 2)])),
        r#"{"1": 1, "2": 2, "1": 3}"#,
    );
}

#[test]
fn duplicate_value_first_wins_hashset() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(HashSet<W>);
    // struct S(#[serde(with = "::serde_with::rust::sets_first_value_wins")] HashSet<W>);

    #[derive(Debug, Eq, Deserialize, Serialize)]
    struct W(i32, bool);
    impl PartialEq for W {
        fn eq(&self, other: &Self) -> bool {
            self.0 == other.0
        }
    }
    impl std::hash::Hash for W {
        fn hash<H>(&self, state: &mut H)
        where
            H: std::hash::Hasher,
        {
            self.0.hash(state)
        }
    }

    // Different values always work
    is_equal(
        S(HashSet::from_iter(vec![
            W(1, true),
            W(2, false),
            W(3, true),
        ])),
        expect![[r#"
            [
              [
                1,
                true
              ],
              [
                3,
                true
              ],
              [
                2,
                false
              ]
            ]"#]],
    );

    let value: S = serde_json::from_str(
        r#"[
        [1, false],
        [1, true],
        [2, true],
        [2, false]
    ]"#,
    )
    .unwrap();
    let entries: Vec<_> = value.0.into_iter().collect();
    assert_eq!(1, entries[0].0);
    assert!(!entries[0].1);
    assert_eq!(2, entries[1].0);
    assert!(entries[1].1);
}

#[test]
fn duplicate_value_last_wins_hashset() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::sets_last_value_wins")] HashSet<W>);

    #[derive(Debug, Eq, Deserialize, Serialize)]
    struct W(i32, bool);
    impl PartialEq for W {
        fn eq(&self, other: &Self) -> bool {
            self.0 == other.0
        }
    }
    impl std::hash::Hash for W {
        fn hash<H>(&self, state: &mut H)
        where
            H: std::hash::Hasher,
        {
            self.0.hash(state)
        }
    }

    // Different values always work
    is_equal(
        S(HashSet::from_iter(vec![
            W(1, true),
            W(2, false),
            W(3, true),
        ])),
        expect![[r#"
            [
              [
                1,
                true
              ],
              [
                3,
                true
              ],
              [
                2,
                false
              ]
            ]"#]],
    );

    let value: S = serde_json::from_str(
        r#"[
        [1, false],
        [1, true],
        [2, true],
        [2, false]
    ]"#,
    )
    .unwrap();
    let entries: Vec<_> = value.0.into_iter().collect();
    assert_eq!(1, entries[0].0);
    assert!(entries[0].1);
    assert_eq!(2, entries[1].0);
    assert!(!entries[1].1);
}

#[test]
fn duplicate_value_last_wins_btreeset() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "::serde_with::rust::sets_last_value_wins")] BTreeSet<W>);
    #[derive(Debug, Eq, Deserialize, Serialize)]
    struct W(i32, bool);
    impl PartialEq for W {
        fn eq(&self, other: &Self) -> bool {
            self.0 == other.0
        }
    }
    impl Ord for W {
        fn cmp(&self, other: &Self) -> cmp::Ordering {
            self.0.cmp(&other.0)
        }
    }
    impl PartialOrd for W {
        fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
            Some(self.cmp(other))
        }
    }

    // Different values always work
    is_equal(
        S(BTreeSet::from_iter(vec![
            W(1, true),
            W(2, false),
            W(3, true),
        ])),
        expect![[r#"
            [
              [
                1,
                true
              ],
              [
                2,
                false
              ],
              [
                3,
                true
              ]
            ]"#]],
    );

    let value: S = serde_json::from_str(
        r#"[
        [1, false],
        [1, true],
        [2, true],
        [2, false]
    ]"#,
    )
    .unwrap();
    let entries: Vec<_> = value.0.into_iter().collect();
    assert_eq!(1, entries[0].0);
    assert!(entries[0].1);
    assert_eq!(2, entries[1].0);
    assert!(!entries[1].1);
}

#[test]
fn test_map_as_tuple_list() {
    #[derive(Debug, Deserialize, Serialize, PartialEq)]
    struct Hash(#[serde(with = "serde_with::rust::map_as_tuple_list")] HashMap<String, u8>);

    is_equal(
        Hash(HashMap::from_iter(vec![
            ("ABC".to_string(), 1),
            ("Hello".to_string(), 0),
            ("World".to_string(), 20),
        ])),
        expect![[r#"
            [
              [
                "ABC",
                1
              ],
              [
                "Hello",
                0
              ],
              [
                "World",
                20
              ]
            ]"#]],
    );
    is_equal(
        Hash(HashMap::from_iter(vec![("Hello".to_string(), 0)])),
        expect![[r#"
            [
              [
                "Hello",
                0
              ]
            ]"#]],
    );
    is_equal(Hash(HashMap::default()), expect![[r#"[]"#]]);

    // Test parse error, only single element instead of tuple
    check_error_deserialization::<Hash>(
        r#"[ [1] ]"#,
        expect![[r#"invalid type: integer `1`, expected a string at line 1 column 4"#]],
    );

    #[derive(Debug, Deserialize, Serialize, PartialEq)]
    struct BTree(#[serde(with = "serde_with::rust::map_as_tuple_list")] BTreeMap<String, u8>);

    is_equal(
        BTree(BTreeMap::from_iter(vec![
            ("ABC".to_string(), 1),
            ("Hello".to_string(), 0),
            ("World".to_string(), 20),
        ])),
        expect![[r#"
            [
              [
                "ABC",
                1
              ],
              [
                "Hello",
                0
              ],
              [
                "World",
                20
              ]
            ]"#]],
    );
    is_equal(
        BTree(BTreeMap::from_iter(vec![("Hello".to_string(), 0)])),
        expect![[r#"
            [
              [
                "Hello",
                0
              ]
            ]"#]],
    );
    is_equal(BTree(BTreeMap::default()), expect![[r#"[]"#]]);

    // Test parse error, only single element instead of tuple
    check_error_deserialization::<BTree>(
        r#"[ [1] ]"#,
        expect![[r#"invalid type: integer `1`, expected a string at line 1 column 4"#]],
    );
}

#[test]
fn tuple_list_as_map_vec() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "serde_with::rust::tuple_list_as_map")] Vec<(Wrapper<i32>, Wrapper<String>)>,
    );
    #[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    is_equal(
        S(vec![
            (Wrapper(1), Wrapper("Hi".into())),
            (Wrapper(2), Wrapper("Cake".into())),
            (Wrapper(99), Wrapper("Lie".into())),
        ]),
        expect![[r#"
            {
              "1": "Hi",
              "2": "Cake",
              "99": "Lie"
            }"#]],
    );
    is_equal(S(Vec::new()), expect![[r#"{}"#]]);
    check_error_deserialization::<S>(
        r#"[]"#,
        expect![[r#"invalid type: sequence, expected a map at line 1 column 0"#]],
    );
    check_error_deserialization::<S>(
        r#"null"#,
        expect![[r#"invalid type: null, expected a map at line 1 column 4"#]],
    );
}

#[test]
fn tuple_list_as_map_linkedlist() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "serde_with::rust::tuple_list_as_map")]
        LinkedList<(Wrapper<i32>, Wrapper<String>)>,
    );
    #[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    is_equal(
        S(LinkedList::from_iter(vec![
            (Wrapper(1), Wrapper("Hi".into())),
            (Wrapper(2), Wrapper("Cake".into())),
            (Wrapper(99), Wrapper("Lie".into())),
        ])),
        expect![[r#"
            {
              "1": "Hi",
              "2": "Cake",
              "99": "Lie"
            }"#]],
    );
    is_equal(S(LinkedList::new()), expect![[r#"{}"#]]);
    check_error_deserialization::<S>(
        r#"[]"#,
        expect![[r#"invalid type: sequence, expected a map at line 1 column 0"#]],
    );
    check_error_deserialization::<S>(
        r#"null"#,
        expect![[r#"invalid type: null, expected a map at line 1 column 4"#]],
    );
}

#[test]
fn tuple_list_as_map_vecdeque() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(
        #[serde(with = "serde_with::rust::tuple_list_as_map")]
        VecDeque<(Wrapper<i32>, Wrapper<String>)>,
    );
    #[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    is_equal(
        S(VecDeque::from_iter(vec![
            (Wrapper(1), Wrapper("Hi".into())),
            (Wrapper(2), Wrapper("Cake".into())),
            (Wrapper(99), Wrapper("Lie".into())),
        ])),
        expect![[r#"
            {
              "1": "Hi",
              "2": "Cake",
              "99": "Lie"
            }"#]],
    );
    is_equal(S(VecDeque::new()), expect![[r#"{}"#]]);
    check_error_deserialization::<S>(
        r#"[]"#,
        expect![[r#"invalid type: sequence, expected a map at line 1 column 0"#]],
    );
    check_error_deserialization::<S>(
        r#"null"#,
        expect![[r#"invalid type: null, expected a map at line 1 column 4"#]],
    );
}

#[test]
fn test_string_empty_as_none() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S(#[serde(with = "serde_with::rust::string_empty_as_none")] Option<String>);

    is_equal(S(Some("str".to_string())), expect![[r#""str""#]]);
    check_deserialization(S(None), r#""""#);
    check_deserialization(S(None), r#"null"#);
}

#[test]
fn test_default_on_error() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S<T>(#[serde(with = "serde_with::rust::default_on_error")] T)
    where
        T: Default + Serialize + DeserializeOwned;

    is_equal(S(123), expect![[r#"123"#]]);
    is_equal(S("Hello World".to_string()), expect![[r#""Hello World""#]]);
    is_equal(
        S(vec![1, 2, 3]),
        expect![[r#"
        [
          1,
          2,
          3
        ]"#]],
    );

    check_deserialization(S(0), r#"{}"#);
    check_deserialization(S(0), r#"[]"#);
    check_deserialization(S(0), r#"null"#);
    check_deserialization(S(0), r#""A""#);

    check_deserialization(S("".to_string()), r#"{}"#);
    check_deserialization(S("".to_string()), r#"[]"#);
    check_deserialization(S("".to_string()), r#"null"#);
    check_deserialization(S("".to_string()), r#"0"#);

    check_deserialization(S::<Vec<i32>>(vec![]), r#"{}"#);
    check_deserialization(S::<Vec<i32>>(vec![]), r#"null"#);
    check_deserialization(S::<Vec<i32>>(vec![]), r#"0"#);
    check_deserialization(S::<Vec<i32>>(vec![]), r#""A""#);
}

#[test]
fn test_default_on_null() {
    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct S<T>(#[serde(with = "serde_with::rust::default_on_null")] T)
    where
        T: Default + Serialize + DeserializeOwned;

    is_equal(S(123), expect![[r#"123"#]]);
    is_equal(S("Hello World".to_string()), expect![[r#""Hello World""#]]);
    is_equal(
        S(vec![1, 2, 3]),
        expect![[r#"
        [
          1,
          2,
          3
        ]"#]],
    );

    check_deserialization(S(0), r#"null"#);
    check_deserialization(S("".to_string()), r#"null"#);
    check_deserialization(S::<Vec<i32>>(vec![]), r#"null"#);
}
