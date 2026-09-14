variable "aws_region" {
  type    = string
  default = "us-west-2"
}

variable "instance_type" {
  description = "c7i-flex.large provides 2 vCPU/4 GiB and is a good single-node K3s demo choice when it is Free-Tier-eligible/ available for your account and region. Verify before apply."
  type        = string
  default     = "c7i-flex.large"
}

variable "github_repository" {
  description = "GitHub repository in OWNER/REPO form. The repo should be public for anonymous GHCR/image and git access."
  type        = string
}

variable "github_owner_id" {
  description = "Immutable numeric GitHub owner ID used by post-July-15-2026 OIDC subject claims."
  type        = string
}

variable "github_repository_id" {
  description = "Immutable numeric GitHub repository ID used by post-July-15-2026 OIDC subject claims."
  type        = string
}

variable "github_branch" {
  type    = string
  default = "main"
}

variable "root_volume_gb" {
  type    = number
  default = 20
}
