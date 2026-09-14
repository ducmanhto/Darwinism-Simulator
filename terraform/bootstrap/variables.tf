variable "aws_region" {
  type    = string
  default = "us-west-2"
}

variable "state_bucket_name" {
  description = "Globally unique S3 bucket name for DarwinSim Terraform state."
  type        = string
}
