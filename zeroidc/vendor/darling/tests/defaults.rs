use darling::FromDeriveInput;
use syn::parse_quote;

mod foo {
    pub mod bar {
        pub fn init() -> String {
            String::from("hello")
        }
    }
}

#[derive(FromDeriveInput)]
#[darling(attributes(speak))]
pub struct SpeakerOpts {
    #[darling(default = "foo::bar::init")]
    first_word: String,
}

#[test]
fn path_default() {
    let speaker: SpeakerOpts = FromDeriveInput::from_derive_input(&parse_quote! {
        struct Foo;
    })
    .expect("Unit struct with no attrs should parse");

    assert_eq!(speaker.first_word, "hello");
}

/// Tests in this module capture the somewhat-confusing behavior observed when defaults
/// are set at both the field and container level.
///
/// The general rule is that more-specific declarations preempt less-specific ones; this is
/// unsurprising and allows for granular control over what happens when parsing an AST.
mod stacked_defaults {
    use darling::{FromDeriveInput, FromMeta};
    use syn::parse_quote;

    fn jane() -> String {
        "Jane".into()
    }

    #[derive(FromMeta)]
    #[darling(default)]
    struct PersonName {
        #[darling(default = "jane")]
        first: String,
        #[darling(default)]
        middle: String,
        last: String,
    }

    impl Default for PersonName {
        fn default() -> Self {
            Self {
                first: "John".into(),
                middle: "T".into(),
                last: "Doe".into(),
            }
        }
    }

    #[derive(FromDeriveInput)]
    #[darling(attributes(person))]
    struct Person {
        #[darling(default)]
        name: PersonName,
        age: u8,
    }

    #[test]
    fn name_first_only() {
        let person = Person::from_derive_input(&parse_quote! {
            #[person(name(first = "Bill"), age = 5)]
            struct Foo;
        })
        .unwrap();

        assert_eq!(person.name.first, "Bill");
        assert_eq!(
            person.name.middle, "",
            "Explicit field-level default should preempt container-level default"
        );
        assert_eq!(
            person.name.last, "Doe",
            "Absence of a field-level default falls back to container-level default"
        );
    }

    /// This is the most surprising case. The presence of `name()` means we invoke
    /// `PersonName::from_list(&[])`. When that finishes parsing each of the zero nested
    /// items it has received, it will then start filling in missing fields, using the
    /// explicit field-level defaults for `first` and `middle`, while for `last` it will
    /// use the `last` field from the container-level default.
    #[test]
    fn name_empty_list() {
        let person = Person::from_derive_input(&parse_quote! {
            #[person(name(), age = 5)]
            struct Foo;
        })
        .unwrap();

        assert_eq!(person.name.first, "Jane");
        assert_eq!(person.name.middle, "");
        assert_eq!(person.name.last, "Doe");
    }

    #[test]
    fn no_name() {
        let person = Person::from_derive_input(&parse_quote! {
            #[person(age = 5)]
            struct Foo;
        })
        .unwrap();

        assert_eq!(person.age, 5);
        assert_eq!(
            person.name.first, "John",
            "If `name` is not specified, `Person`'s field-level default should be used"
        );
        assert_eq!(person.name.middle, "T");
        assert_eq!(person.name.last, "Doe");
    }
}
