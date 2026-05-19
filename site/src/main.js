import "./styles.css";

const copyButton = document.querySelector("[data-copy]");

if (copyButton) {
  copyButton.addEventListener("click", async () => {
    const command = copyButton.getAttribute("data-copy") ?? "";
    try {
      await navigator.clipboard.writeText(command);
      copyButton.textContent = "Copied";
      window.setTimeout(() => {
        copyButton.textContent = "Copy";
      }, 1600);
    } catch {
      copyButton.textContent = command;
    }
  });
}

const video = document.querySelector(".hero-video");

if (video) {
  video.addEventListener("error", () => {
    video.classList.add("is-unavailable");
  });
}
