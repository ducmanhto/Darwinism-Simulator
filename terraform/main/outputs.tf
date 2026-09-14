output "instance_id" {
  value = aws_instance.darwinsim.id
}

output "public_ip" {
  value = aws_instance.darwinsim.public_ip
}

output "web_url" {
  value = "http://${aws_instance.darwinsim.public_ip}"
}

output "github_deploy_role_arn" {
  value = aws_iam_role.github_deploy.arn
}

output "github_oidc_subject" {
  value = local.github_oidc_sub
}
