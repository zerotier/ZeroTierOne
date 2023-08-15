/*
 * Copyright (c)2023 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

pub mod ext;

use serde::{Deserialize, Serialize};
use std::str::FromStr;
use std::time::Duration;
use temporal_client::{Client, ClientOptionsBuilder, RetryClient, WorkflowClientTrait, WorkflowOptions};
use temporal_sdk_core_protos::{coresdk::AsJsonPayloadExt, temporal::api::enums::v1::WorkflowIdReusePolicy};
use url::Url;
use uuid::Uuid;

const CLIENT_NAME: &str = "SmeeClient-Rust";
const CLIENT_VERSION: &str = "0.1";
const NETWORK_JOINED_WORKFLOW: &str = "NetworkJoinedWorkflow";

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct NetworkJoinedParams {
    #[serde(rename = "NetworkID")]
    pub network_id: String,

    #[serde(rename = "MemberID")]
    pub member_id: String,

    #[serde(rename = "HookID")]
    pub hook_id: String,

    #[serde(rename = "SrcIP")]
    pub src_ip: Option<String>,
}

impl NetworkJoinedParams {
    fn new(network_id: &str, member_id: &str, hook_id: &str, src_ip: Option<&str>) -> Self {
        Self {
            network_id: network_id.to_string(),
            member_id: member_id.to_string(),
            hook_id: hook_id.to_string(),
            src_ip: match src_ip {
                Some(x) => Some(x.to_string()),
                None => None,
            },
        }
    }
}

pub struct SmeeClient {
    tokio_rt: tokio::runtime::Runtime,
    client: RetryClient<Client>,
    task_queue: String,
}

impl SmeeClient {
    pub fn new(temporal_url: &str, namespace: &str, task_queue: &str) -> Result<Self, Box<dyn std::error::Error>> {
        // start tokio runtime.  Required by temporal
        let rt = tokio::runtime::Runtime::new()?;

        let c = ClientOptionsBuilder::default()
            .target_url(Url::from_str(temporal_url).unwrap())
            .client_name(CLIENT_NAME)
            .client_version(CLIENT_VERSION)
            .build()?;

        let con = rt.block_on(async { c.connect(namespace.to_string(), None, None).await })?;

        Ok(Self {
            tokio_rt: rt,
            client: con,
            task_queue: task_queue.to_string(),
        })
    }

    pub fn notify_network_joined(&self, params: NetworkJoinedParams) -> Result<(), Box<dyn std::error::Error>> {
        let options = WorkflowOptions {
            id_reuse_policy: WorkflowIdReusePolicy::RejectDuplicate,
            execution_timeout: None,
            run_timeout: None,
            task_timeout: None,
            cron_schedule: None,
            search_attributes: None,
        };

        let payload = vec![params.as_json_payload()?];

        let workflow_id = Uuid::new_v4();

        self.tokio_rt.block_on(async {
            self.client
                .start_workflow(
                    payload,
                    self.task_queue.clone(),
                    workflow_id.hyphenated().to_string(),
                    String::from(NETWORK_JOINED_WORKFLOW),
                    None,
                    options,
                )
                .await
        })?;

        Ok(())
    }

    pub fn shutdown(self) {
        self.tokio_rt.shutdown_timeout(Duration::from_secs(5))
    }
}
