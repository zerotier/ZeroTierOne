/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace gen_ai
{

/**
  Free-form description of the GenAI agent provided by the application.
 */
static constexpr const char *kGenAiAgentDescription = "gen_ai.agent.description";

/**
  The unique identifier of the GenAI agent.
 */
static constexpr const char *kGenAiAgentId = "gen_ai.agent.id";

/**
  Human-readable name of the GenAI agent provided by the application.
 */
static constexpr const char *kGenAiAgentName = "gen_ai.agent.name";

/**
  Deprecated, use Event API to report completions contents.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiCompletion = "gen_ai.completion";

/**
  The unique identifier for a conversation (session, thread), used to store and correlate messages
  within this conversation.
 */
static constexpr const char *kGenAiConversationId = "gen_ai.conversation.id";

/**
  The data source identifier.
  <p>
  Data sources are used by AI agents and RAG applications to store grounding data. A data source may
  be an external database, object store, document collection, website, or any other storage system
  used by the GenAI agent or application. The @code gen_ai.data_source.id @endcode SHOULD match the
  identifier used by the GenAI system rather than a name specific to the external storage, such as a
  database or object store. Semantic conventions referencing @code gen_ai.data_source.id @endcode
  MAY also leverage additional attributes, such as @code db.* @endcode, to further identify and
  describe the data source.
 */
static constexpr const char *kGenAiDataSourceId = "gen_ai.data_source.id";

/**
  Deprecated, use @code gen_ai.output.type @endcode.

  @deprecated
  {"note": "Replaced by @code gen_ai.output.type @endcode.", "reason": "renamed", "renamed_to":
  "gen_ai.output.type"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiOpenaiRequestResponseFormat =
    "gen_ai.openai.request.response_format";

/**
  Deprecated, use @code gen_ai.request.seed @endcode.

  @deprecated
  {"note": "Replaced by @code gen_ai.request.seed @endcode.", "reason": "renamed", "renamed_to":
  "gen_ai.request.seed"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiOpenaiRequestSeed =
    "gen_ai.openai.request.seed";

/**
  The service tier requested. May be a specific tier, default, or auto.
 */
static constexpr const char *kGenAiOpenaiRequestServiceTier = "gen_ai.openai.request.service_tier";

/**
  The service tier used for the response.
 */
static constexpr const char *kGenAiOpenaiResponseServiceTier =
    "gen_ai.openai.response.service_tier";

/**
  A fingerprint to track any eventual change in the Generative AI environment.
 */
static constexpr const char *kGenAiOpenaiResponseSystemFingerprint =
    "gen_ai.openai.response.system_fingerprint";

/**
  The name of the operation being performed.
  <p>
  If one of the predefined values applies, but specific system uses a different name it's
  RECOMMENDED to document it in the semantic conventions for specific GenAI system and use
  system-specific name in the instrumentation. If a different name is not documented,
  instrumentation libraries SHOULD use applicable predefined value.
 */
static constexpr const char *kGenAiOperationName = "gen_ai.operation.name";

/**
  Represents the content type requested by the client.
  <p>
  This attribute SHOULD be used when the client requests output of a specific type. The model may
  return zero or more outputs of this type. This attribute specifies the output modality and not the
  actual output format. For example, if an image is requested, the actual output could be a URL
  pointing to an image file. Additional output format details may be recorded in the future in the
  @code gen_ai.output.{type}.* @endcode attributes.
 */
static constexpr const char *kGenAiOutputType = "gen_ai.output.type";

/**
  Deprecated, use Event API to report prompt contents.

  @deprecated
  {"note": "Removed, no replacement at this time.", "reason": "obsoleted"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiPrompt = "gen_ai.prompt";

/**
  The target number of candidate completions to return.
 */
static constexpr const char *kGenAiRequestChoiceCount = "gen_ai.request.choice.count";

/**
  The encoding formats requested in an embeddings operation, if specified.
  <p>
  In some GenAI systems the encoding formats are called embedding types. Also, some GenAI systems
  only accept a single format per request.
 */
static constexpr const char *kGenAiRequestEncodingFormats = "gen_ai.request.encoding_formats";

/**
  The frequency penalty setting for the GenAI request.
 */
static constexpr const char *kGenAiRequestFrequencyPenalty = "gen_ai.request.frequency_penalty";

/**
  The maximum number of tokens the model generates for a request.
 */
static constexpr const char *kGenAiRequestMaxTokens = "gen_ai.request.max_tokens";

/**
  The name of the GenAI model a request is being made to.
 */
static constexpr const char *kGenAiRequestModel = "gen_ai.request.model";

/**
  The presence penalty setting for the GenAI request.
 */
static constexpr const char *kGenAiRequestPresencePenalty = "gen_ai.request.presence_penalty";

/**
  Requests with same seed value more likely to return same result.
 */
static constexpr const char *kGenAiRequestSeed = "gen_ai.request.seed";

/**
  List of sequences that the model will use to stop generating further tokens.
 */
static constexpr const char *kGenAiRequestStopSequences = "gen_ai.request.stop_sequences";

/**
  The temperature setting for the GenAI request.
 */
static constexpr const char *kGenAiRequestTemperature = "gen_ai.request.temperature";

/**
  The top_k sampling setting for the GenAI request.
 */
static constexpr const char *kGenAiRequestTopK = "gen_ai.request.top_k";

/**
  The top_p sampling setting for the GenAI request.
 */
static constexpr const char *kGenAiRequestTopP = "gen_ai.request.top_p";

/**
  Array of reasons the model stopped generating tokens, corresponding to each generation received.
 */
static constexpr const char *kGenAiResponseFinishReasons = "gen_ai.response.finish_reasons";

/**
  The unique identifier for the completion.
 */
static constexpr const char *kGenAiResponseId = "gen_ai.response.id";

/**
  The name of the model that generated the response.
 */
static constexpr const char *kGenAiResponseModel = "gen_ai.response.model";

/**
  The Generative AI product as identified by the client or server instrumentation.
  <p>
  The @code gen_ai.system @endcode describes a family of GenAI models with specific model identified
  by @code gen_ai.request.model @endcode and @code gen_ai.response.model @endcode attributes.
  <p>
  The actual GenAI product may differ from the one identified by the client.
  Multiple systems, including Azure OpenAI and Gemini, are accessible by OpenAI client
  libraries. In such cases, the @code gen_ai.system @endcode is set to @code openai @endcode based
  on the instrumentation's best knowledge, instead of the actual system. The @code server.address
  @endcode attribute may help identify the actual system in use for @code openai @endcode. <p> For
  custom model, a custom friendly name SHOULD be used. If none of these options apply, the @code
  gen_ai.system @endcode SHOULD be set to @code _OTHER @endcode.
 */
static constexpr const char *kGenAiSystem = "gen_ai.system";

/**
  The type of token being counted.
 */
static constexpr const char *kGenAiTokenType = "gen_ai.token.type";

/**
  The tool call identifier.
 */
static constexpr const char *kGenAiToolCallId = "gen_ai.tool.call.id";

/**
  The tool description.
 */
static constexpr const char *kGenAiToolDescription = "gen_ai.tool.description";

/**
  Name of the tool utilized by the agent.
 */
static constexpr const char *kGenAiToolName = "gen_ai.tool.name";

/**
  Type of the tool utilized by the agent
  <p>
  Extension: A tool executed on the agent-side to directly call external APIs, bridging the gap
  between the agent and real-world systems. Agent-side operations involve actions that are performed
  by the agent on the server or within the agent's controlled environment. Function: A tool executed
  on the client-side, where the agent generates parameters for a predefined function, and the client
  executes the logic. Client-side operations are actions taken on the user's end or within the
  client application. Datastore: A tool used by the agent to access and query structured or
  unstructured external data for retrieval-augmented tasks or knowledge updates.
 */
static constexpr const char *kGenAiToolType = "gen_ai.tool.type";

/**
  Deprecated, use @code gen_ai.usage.output_tokens @endcode instead.

  @deprecated
  {"note": "Replaced by @code gen_ai.usage.output_tokens @endcode.", "reason": "renamed",
  "renamed_to": "gen_ai.usage.output_tokens"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiUsageCompletionTokens =
    "gen_ai.usage.completion_tokens";

/**
  The number of tokens used in the GenAI input (prompt).
 */
static constexpr const char *kGenAiUsageInputTokens = "gen_ai.usage.input_tokens";

/**
  The number of tokens used in the GenAI response (completion).
 */
static constexpr const char *kGenAiUsageOutputTokens = "gen_ai.usage.output_tokens";

/**
  Deprecated, use @code gen_ai.usage.input_tokens @endcode instead.

  @deprecated
  {"note": "Replaced by @code gen_ai.usage.input_tokens @endcode.", "reason": "renamed",
  "renamed_to": "gen_ai.usage.input_tokens"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kGenAiUsagePromptTokens =
    "gen_ai.usage.prompt_tokens";

namespace GenAiOpenaiRequestResponseFormatValues
{
/**
  Text response format
 */
static constexpr const char *kText = "text";

/**
  JSON object response format
 */
static constexpr const char *kJsonObject = "json_object";

/**
  JSON schema response format
 */
static constexpr const char *kJsonSchema = "json_schema";

}  // namespace GenAiOpenaiRequestResponseFormatValues

namespace GenAiOpenaiRequestServiceTierValues
{
/**
  The system will utilize scale tier credits until they are exhausted.
 */
static constexpr const char *kAuto = "auto";

/**
  The system will utilize the default scale tier.
 */
static constexpr const char *kDefault = "default";

}  // namespace GenAiOpenaiRequestServiceTierValues

namespace GenAiOperationNameValues
{
/**
  Chat completion operation such as <a
  href="https://platform.openai.com/docs/api-reference/chat">OpenAI Chat API</a>
 */
static constexpr const char *kChat = "chat";

/**
  Multimodal content generation operation such as <a
  href="https://ai.google.dev/api/generate-content">Gemini Generate Content</a>
 */
static constexpr const char *kGenerateContent = "generate_content";

/**
  Text completions operation such as <a
  href="https://platform.openai.com/docs/api-reference/completions">OpenAI Completions API
  (Legacy)</a>
 */
static constexpr const char *kTextCompletion = "text_completion";

/**
  Embeddings operation such as <a
  href="https://platform.openai.com/docs/api-reference/embeddings/create">OpenAI Create embeddings
  API</a>
 */
static constexpr const char *kEmbeddings = "embeddings";

/**
  Create GenAI agent
 */
static constexpr const char *kCreateAgent = "create_agent";

/**
  Invoke GenAI agent
 */
static constexpr const char *kInvokeAgent = "invoke_agent";

/**
  Execute a tool
 */
static constexpr const char *kExecuteTool = "execute_tool";

}  // namespace GenAiOperationNameValues

namespace GenAiOutputTypeValues
{
/**
  Plain text
 */
static constexpr const char *kText = "text";

/**
  JSON object with known or unknown schema
 */
static constexpr const char *kJson = "json";

/**
  Image
 */
static constexpr const char *kImage = "image";

/**
  Speech
 */
static constexpr const char *kSpeech = "speech";

}  // namespace GenAiOutputTypeValues

namespace GenAiSystemValues
{
/**
  OpenAI
 */
static constexpr const char *kOpenai = "openai";

/**
  Any Google generative AI endpoint
 */
static constexpr const char *kGcpGenAi = "gcp.gen_ai";

/**
  Vertex AI
 */
static constexpr const char *kGcpVertexAi = "gcp.vertex_ai";

/**
  Gemini
 */
static constexpr const char *kGcpGemini = "gcp.gemini";

/**
  Vertex AI
 */
static constexpr const char *kVertexAi = "vertex_ai";

/**
  Gemini
 */
static constexpr const char *kGemini = "gemini";

/**
  Anthropic
 */
static constexpr const char *kAnthropic = "anthropic";

/**
  Cohere
 */
static constexpr const char *kCohere = "cohere";

/**
  Azure AI Inference
 */
static constexpr const char *kAzAiInference = "az.ai.inference";

/**
  Azure OpenAI
 */
static constexpr const char *kAzAiOpenai = "az.ai.openai";

/**
  IBM Watsonx AI
 */
static constexpr const char *kIbmWatsonxAi = "ibm.watsonx.ai";

/**
  AWS Bedrock
 */
static constexpr const char *kAwsBedrock = "aws.bedrock";

/**
  Perplexity
 */
static constexpr const char *kPerplexity = "perplexity";

/**
  xAI
 */
static constexpr const char *kXai = "xai";

/**
  DeepSeek
 */
static constexpr const char *kDeepseek = "deepseek";

/**
  Groq
 */
static constexpr const char *kGroq = "groq";

/**
  Mistral AI
 */
static constexpr const char *kMistralAi = "mistral_ai";

}  // namespace GenAiSystemValues

namespace GenAiTokenTypeValues
{
/**
  Input tokens (prompt, input, etc.)
 */
static constexpr const char *kInput = "input";

/**
  Output tokens (completion, response, etc.)
 */
static constexpr const char *kCompletion = "output";

/**
  Output tokens (completion, response, etc.)
 */
static constexpr const char *kOutput = "output";

}  // namespace GenAiTokenTypeValues

}  // namespace gen_ai
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
