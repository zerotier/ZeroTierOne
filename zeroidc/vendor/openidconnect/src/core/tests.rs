use super::CoreGrantType;

#[test]
fn test_grant_type_serialize() {
    let serialized_implicit = serde_json::to_string(&CoreGrantType::Implicit).unwrap();
    assert_eq!("\"implicit\"", serialized_implicit);
    assert_eq!(
        CoreGrantType::Implicit,
        serde_json::from_str::<CoreGrantType>(&serialized_implicit).unwrap()
    );
}
