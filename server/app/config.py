"""服务配置（环境变量）。

数据库固定使用 MySQL（异步驱动 aiomysql）。
连接串示例：
  MCU_DATABASE_URL=mysql+aiomysql://user:pass@127.0.0.1:3306/mcu_skills?charset=utf8mb4
"""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path
from urllib.parse import quote_plus

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


def _default_repo_root() -> Path:
    # server/app/config.py -> 仓库根
    return Path(__file__).resolve().parents[2]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="MCU_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    skills_root: Path = Field(default_factory=_default_repo_root)
    data_dir: Path = Field(default_factory=lambda: _default_repo_root() / "data")

    # MySQL 连接（优先完整 URL；否则用分项拼装）
    database_url: str = ""
    db_host: str = "127.0.0.1"
    db_port: int = 3306
    db_user: str = "mcu_skills"
    db_password: str = ""
    db_name: str = "mcu_skills"
    db_charset: str = "utf8mb4"

    bootstrap_admin_key: str = ""
    reindex_cron: str = "0 2 * * *"
    workspace_allowlist: str = ""
    embedding_provider: str = "none"
    max_resource_bytes: int = 512_000
    session_secret: str = "change-me-in-production"
    host: str = "0.0.0.0"
    port: int = 8080

    def model_post_init(self, __context: object) -> None:
        if not self.database_url:
            user = quote_plus(self.db_user)
            password = quote_plus(self.db_password)
            auth = f"{user}:{password}@" if self.db_password else f"{user}@"
            url = (
                f"mysql+aiomysql://{auth}{self.db_host}:{self.db_port}/"
                f"{self.db_name}?charset={self.db_charset}"
            )
            object.__setattr__(self, "database_url", url)

    @property
    def uploads_dir(self) -> Path:
        return self.data_dir / "uploads"

    @property
    def index_dir(self) -> Path:
        return self.data_dir / "index"

    def ensure_dirs(self) -> None:
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.uploads_dir.mkdir(parents=True, exist_ok=True)
        self.index_dir.mkdir(parents=True, exist_ok=True)

    def allowlist_roots(self) -> list[Path]:
        raw = [p.strip() for p in self.workspace_allowlist.split(",") if p.strip()]
        if not raw:
            return [self.skills_root.resolve()]
        return [Path(p).expanduser().resolve() for p in raw]


@lru_cache
def get_settings() -> Settings:
    return Settings()
